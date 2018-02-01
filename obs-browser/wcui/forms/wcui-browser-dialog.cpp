#include "wcui-browser-dialog.hpp"
#include "ui_wcui-browser-dialog.h"
#include "browser-manager.hpp"
#include "browser-obs-bridge-base.hpp"

#include "include/cef_parser.h"		// CefParseJSON, CefWriteJSON

#include "fmt/format.h"

#include "obs.h"
#include "obs-encoder.h"

#include <QWidget>
#include <QFrame>

WCUIBrowserDialog::WCUIBrowserDialog(QWidget* parent, std::string obs_module_path, std::string cache_path) :
	QDialog(parent, Qt::Dialog),
	ui(new Ui::WCUIBrowserDialog),
	m_obs_module_path(obs_module_path),
	m_cache_path(cache_path)
{
	setAttribute(Qt::WA_NativeWindow);

	ui->setupUi(this);

	// Remove help question mark from title bar
	setWindowFlags(windowFlags() &= ~Qt::WindowContextHelpButtonHint);
}

WCUIBrowserDialog::~WCUIBrowserDialog()
{
	delete ui;
}

void WCUIBrowserDialog::ShowModal()
{
	// Get window handle of CEF container frame
	auto frame = findChild<QFrame*>("frame");
	m_window_handle = (cef_window_handle_t)frame->winId();

	// Spawn CEF initialization in new thread.
	//
	// The window handle must be obtained in the QT UI thread and CEF initialization must be performed in a
	// separate thread, otherwise a dead lock occurs and everything just hangs.
	//
	m_task_queue.Enqueue(
		[](void* args)
		{
			WCUIBrowserDialog* self = (WCUIBrowserDialog*)args;

			self->InitBrowser();
		},
		this);

	// Start modal dialog
	exec();
}

// Initialize CEF.
//
// This function is called in a separate thread by InitBrowserThreadEntryPoint() above.
//
// DO NOT call this function from the QT UI thread: it will lead to a dead lock.
//
void WCUIBrowserDialog::InitBrowser()
{
	std::string absoluteHtmlFilePath;

	// Get initial UI HTML file full path
	std::string parentPath(
		m_obs_module_path.substr(0, m_obs_module_path.find_last_of('/') + 1));

	// Launcher local HTML page path
	std::string htmlPartialPath = parentPath + "/obs-browser-wcui-browser-dialog.html";

#ifdef _WIN32
	char htmlFullPath[MAX_PATH + 1];
	::GetFullPathNameA(htmlPartialPath.c_str(), MAX_PATH, htmlFullPath, NULL);

	absoluteHtmlFilePath = htmlFullPath;
#else
	char* htmlFullPath = realpath(htmlPartialPath.c_str(), NULL);

	absoluteHtmlFilePath = htmlFullPath;

	free(htmlFullPath);
#endif

	// BrowserManager installs a custom http scheme URL handler to access local files.
	//
	// We don't need this on Windows, perhaps MacOS / UNIX users need this?
	//
	CefString url = "http://absolute/" + absoluteHtmlFilePath;

	if (m_browser_handle == BROWSER_HANDLE_NONE)
	{
		// Browser has not been created yet

		// Client area rectangle
		RECT clientRect;

		clientRect.left = 0;
		clientRect.top = 0;
		clientRect.right = width();
		clientRect.bottom = height();

		// CefClient
		CefRefPtr<BrowserClient> client(new BrowserClient(NULL, NULL, new BrowserOBSBridgeBase(), this));
		
		// Window info
		CefWindowInfo window_info;

		CefBrowserSettings settings;

		settings.Reset();

		// Don't allow JavaScript to close the browser window
		settings.javascript_close_windows = STATE_DISABLED;

		window_info.SetAsChild(m_window_handle, clientRect);

		m_browser_handle = BrowserManager::Instance()->CreateBrowser(window_info, client, url, settings, nullptr);
	}
	else
	{
		// Reset URL for browser which has already been created

		BrowserManager::Instance()->LoadURL(m_browser_handle, url);
	}
}

void WCUIBrowserDialog::OnProcessMessageReceivedSendExecuteCallbackMessage(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message,
	CefRefPtr<CefValue> callback_arg)
{
	// Get caller callback ID
	int callbackID = message->GetArgumentList()->GetInt(0);

	// Convert callback argument to JSON
	CefString jsonString =
		CefWriteJSON(callback_arg, JSON_WRITER_DEFAULT);

	// Create "executeCallback" message for the renderer process
	CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("executeCallback");

	// Get callback arguments collection and set callback arguments
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetInt(0, callbackID);	// Callback identifier in renderer process callbackMap

	args->SetString(1, jsonString);	// Callback argument JSON string which will be converted back to CefV8Value in
					// the renderer process

	// Send message to the renderer process
	browser->SendProcessMessage(source_process, msg); // source_process = PID_RENDERER
}

void WCUIBrowserDialog::OnProcessMessageReceivedSendExecuteCallbackMessageForObsEncoderOfType(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message,
	obs_encoder_type encoder_type)
{
	// Response root object
	CefRefPtr<CefValue> root = CefValue::Create();

	// Response codec collection (array)
	CefRefPtr<CefListValue> codec_list = CefListValue::Create();

	// Response codec collection is our root object
	root->SetList(codec_list);

	// Iterate over each available codec
	bool continue_iteration = true;
	for (size_t idx = 0; continue_iteration; ++idx)
	{
		// Set by obs_enum_encoder_types() call below
		const char* encoderId = NULL;

		// Get next encoder ID
		continue_iteration = obs_enum_encoder_types(idx, &encoderId);

		// If obs_enum_encoder_types() returned a result
		if (continue_iteration)
		{
			// If encoder is of correct type (AUDIO / VIDEO)
			if (obs_get_encoder_type(encoderId) == encoder_type)
			{
				CefRefPtr<CefDictionaryValue> codec = CefDictionaryValue::Create();

				// Set codec dictionary properties
				codec->SetString("id", encoderId);
				codec->SetString("codec", obs_get_encoder_codec(encoderId));
				codec->SetString("name", obs_encoder_get_display_name(encoderId));

				// Append dictionary to codec list
				codec_list->SetDictionary(
					codec_list->GetSize(),
					codec);
			}
		}
	}

	// Send message to call calling web page JS callback with codec info as an argument
	OnProcessMessageReceivedSendExecuteCallbackMessage(
		browser,
		source_process,
		message,
		root);
}

// Called when a new message is received from a different process. Return true
// if the message was handled or false otherwise. Do not keep a reference to
// or attempt to access the message outside of this callback.
//
/*--cef()--*/
bool WCUIBrowserDialog::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
	CefProcessId source_process,
	CefRefPtr<CefProcessMessage> message)
{
	// Get message name
	CefString name = message->GetName();

	// Get message arguments
	CefRefPtr<CefListValue> args = message->GetArgumentList();

	if (name == "setupEnvironment" && args->GetSize() > 0)
	{
		// window.obsstudio.setupEnvironment(config) JS call

		CefString config_json_string = args->GetValue(0)->GetString();
		CefRefPtr<CefValue> config =
			CefParseJSON(config_json_string, JSON_PARSER_ALLOW_TRAILING_COMMAS);

		if (config->GetDictionary() != NULL)
		{
			auto input = config->GetDictionary()->GetValue("input");
			auto output = config->GetDictionary()->GetValue("output");

		}

		m_task_queue.Enqueue(
			[](void* arg)
			{
				WCUIBrowserDialog* self = (WCUIBrowserDialog*)arg;

				size_t initSceneCount = self->ObsScenesGetCount();

				self->ObsAddScene(fmt::format("New scene {}", os_gettime_ns()).c_str(), true);
				self->ObsAddSourceBrowser(NULL, "Browser 1", 1920, 1080, 25, "http://www.streamelements.com/");

				self->ObsAddScene(fmt::format("New scene {}", os_gettime_ns()).c_str(), true);
				self->ObsAddSourceGame(NULL, "Game 1");
				self->ObsAddSource(NULL, "monitor_capture", "Monitor 1", NULL, NULL, false);
				self->ObsAddSourceBrowser(NULL, "Browser 1", 1920, 1080, 25, "http://www.google.com/");
				self->ObsAddSourceVideoCapture(NULL, "Video Capture", 10, 10, 320, 180);

				size_t endSceneCount = self->ObsScenesGetCount();

				if (initSceneCount > 0 && initSceneCount < endSceneCount)
				{
					self->ObsRemoveFirstScenes(initSceneCount);
				}
			},
			this);

		/*
		QMetaObject::invokeMethod(
			this,
			"ObsAddScene",
			Q_ARG(const char*, fmt::format("New scene {}", os_gettime_ns()).c_str()),
			Q_ARG(bool, true));
		*/

		/*
		obs_enum_sources(
			[](void* data, obs_source_t* source) {
				::MessageBoxA(0, obs_source_get_name(source), obs_source_get_name(source), 0);
				return true;
			},
			NULL);*/

		return true;
	}
	else if (name == "videoEncoders")
	{
		// window.obsstudio.videoEncoders(callback) JS call

		OnProcessMessageReceivedSendExecuteCallbackMessageForObsEncoderOfType(
			browser,
			source_process,
			message,
			OBS_ENCODER_VIDEO);

		return true;
	}
	else if (name == "audioEncoders")
	{
		// window.obsstudio.audioEncoders(callback) JS call

		OnProcessMessageReceivedSendExecuteCallbackMessageForObsEncoderOfType(
			browser,
			source_process,
			message,
			OBS_ENCODER_AUDIO);

		return true;
	}
	else if (name == "videoInputSources")
	{
		// window.obsstudio.videoInputSources(callback) JS call

		// Response root object
		CefRefPtr<CefValue> root = CefValue::Create();

		// Response codec collection (array)
		CefRefPtr<CefListValue> list = CefListValue::Create();

		// Response codec collection is our root object
		root->SetList(list);

		// Iterate over all input sources
		bool continue_iteration = true;
		for (size_t idx = 0; continue_iteration; ++idx)
		{
			// Filled by obs_enum_input_types() call below
			const char* sourceId;

			// Get next input source type, obs_enum_input_types() returns true as long as
			// there is data at the specified index
			continue_iteration = obs_enum_input_types(idx, &sourceId);

			if (continue_iteration)
			{
				// Get source caps
				uint32_t sourceCaps = obs_get_source_output_flags(sourceId);

				// If source has video
				if (sourceCaps & OBS_SOURCE_VIDEO == OBS_SOURCE_VIDEO)
				{
					// Create source response dictionary
					CefRefPtr<CefDictionaryValue> dic = CefDictionaryValue::Create();

					// Set codec dictionary properties
					dic->SetString("id", sourceId);
					dic->SetString("name", obs_source_get_display_name(sourceId));
					dic->SetBool("hasVideo", sourceCaps & OBS_SOURCE_VIDEO == OBS_SOURCE_VIDEO);
					dic->SetBool("hasAudio", sourceCaps & OBS_SOURCE_AUDIO == OBS_SOURCE_AUDIO);

					// Compare sourceId to known video capture devices
					dic->SetBool("isVideoCaptureDevice",
						strcmp(sourceId, "dshow_input") == 0 ||
						strcmp(sourceId, "decklink-input") == 0);

					// Compare sourceId to known game capture source
					dic->SetBool("isGameCaptureDevice",
						strcmp(sourceId, "game_capture") == 0);

					// Compare sourceId to known browser source
					dic->SetBool("isBrowserSource",
						strcmp(sourceId, "browser_source") == 0);

					// Append dictionary to response list
					list->SetDictionary(
						list->GetSize(),
						dic);
				}
			}
		}

		// Send message to call calling web page JS callback with codec info as an argument
		OnProcessMessageReceivedSendExecuteCallbackMessage(
			browser,
			source_process,
			message,
			root);

		return true;
	}

	return false;
}

void WCUIBrowserDialog::ObsRemoveFirstScenes(size_t removeCount)
{
	struct obs_frontend_source_list scenes = {};

	// Get list of scenes
	obs_frontend_get_scenes(&scenes);

	// For each scene
	for (size_t idx = 0; idx < scenes.sources.num && idx < removeCount; ++idx)
	{
		// Get the scene (a scene is a source)
		obs_source_t* scene = scenes.sources.array[idx];

		// Remove the scene
		obs_source_remove(scene);
	}

	// Free list of scenes.
	// This also calls obs_release_scene() for each scene in the list.
	obs_frontend_source_list_free(&scenes);
}

size_t WCUIBrowserDialog::ObsScenesGetCount()
{
	size_t result = 0;

	struct obs_frontend_source_list scenes = {};

	// Get list of scenes
	obs_frontend_get_scenes(&scenes);

	result = scenes.sources.num;

	// Free list of scenes.
	// This also calls obs_release_scene() for each scene in the list.
	obs_frontend_source_list_free(&scenes);

	return result;
}

void WCUIBrowserDialog::ObsAddScene(const char* name, bool setCurrent)
{
	// Create scene, this will also trigger UI update
	obs_scene_t* scene = obs_scene_create(name);

	if (setCurrent)
	{
		// If setCurrent requested, set the new scene as current scene
		obs_frontend_set_current_scene(obs_scene_get_source(scene));
	}

	// Release reference to new scene
	obs_scene_release(scene);
}

void WCUIBrowserDialog::ObsAddSource(
	obs_source_t* parentScene,
	const char* sourceId,
	const char* sourceName,
	obs_data_t* sourceSettings,
	obs_data_t* sourceHotkeyData,
	bool preferExistingSource,
	obs_source_t** output_source,
	obs_sceneitem_t** output_sceneitem)
{
	bool releaseParentScene = false;

	if (parentScene == NULL)
	{
		parentScene = obs_frontend_get_current_scene();

		releaseParentScene = true;
	}

	obs_source_t* source = NULL;

	if (preferExistingSource)
	{
		// Try locating existing source of the same type for reuse
		//
		// This is especially relevant for video capture sources
		//

		struct enum_sources_args {
			const char* id;
			obs_source_t* result;
		};

		enum_sources_args enum_args = {};
		enum_args.id = sourceId;
		enum_args.result = NULL;

		obs_enum_sources(
			[](void* arg, obs_source_t* iterator)
			{
				enum_sources_args* args = (enum_sources_args*)arg;

				const char* id = obs_source_get_id(iterator);

				if (strcmp(id, args->id) == 0)
				{
					args->result = obs_source_get_ref(iterator);

					return false;
				}

				return true;
			},
			&enum_args);

		source = enum_args.result;
	}

	if (source == NULL)
	{
		// Not reusing an existing source, create a new one
		source = obs_source_create(sourceId, sourceName, sourceSettings, sourceHotkeyData);
	}

	if (source != NULL)
	{
		// Does not increment refcount. No obs_scene_release() call is necessary.
		obs_scene_t* scene = obs_scene_from_source(parentScene);

		struct atomic_update_args {
			obs_source_t* source;
			obs_sceneitem_t* sceneitem;
		};

		atomic_update_args args = {};

		args.source = source;
		args.sceneitem = NULL;

		obs_enter_graphics();
		obs_scene_atomic_update(
			scene,
			[](void *data, obs_scene_t *scene)
			{
				atomic_update_args* args = (atomic_update_args*)data;

				args->sceneitem = obs_scene_add(scene, args->source);
				obs_sceneitem_set_visible(args->sceneitem, true);

				// obs_sceneitem_release??
			},
			&args);
		obs_leave_graphics();

		if (output_sceneitem != NULL)
		{
			obs_sceneitem_addref(args.sceneitem);

			*output_sceneitem = args.sceneitem;
		}

		if (output_source != NULL)
			*output_source = source;
		else
			obs_source_release(source);
	}


	if (releaseParentScene)
	{
		obs_source_release(parentScene);
	}
}

void WCUIBrowserDialog::ObsAddSourceBrowser(
	obs_source_t* parentScene,
	const char* name,
	const long long width,
	const long long height,
	const long long fps,
	const char* url,
	const bool shutdownWhenInactive,
	const char* css)
{
	obs_data_t* settings = obs_data_create();

	obs_data_set_bool(settings, "is_local_file", false);
	obs_data_set_string(settings, "url", url);
	obs_data_set_string(settings, "css", css);
	obs_data_set_int(settings, "width", 1920);
	obs_data_set_int(settings, "height", 1080);
	obs_data_set_int(settings, "fps", 25);
	obs_data_set_bool(settings, "shutdown", true);

	ObsAddSource(parentScene, "browser_source", name, settings, NULL, false);

	obs_data_release(settings);
}

void WCUIBrowserDialog::ObsAddSourceVideoCapture(
	obs_source_t* parentScene,
	const char* name,
	const int x,
	const int y,
	const int maxWidth,
	const int maxHeight)
{
#ifdef _WIN32
	const char* VIDEO_DEVICE_ID = "video_device_id";

	const char* sourceId = "dshow_input";
#else // APPLE / LINUX
	const char* VIDEO_DEVICE_ID = "device";

	const char* sourceId = "av_capture_input";
#endif

	// Get default settings
	obs_data_t* settings = obs_get_source_defaults(sourceId);

	if (settings != NULL)
	{
		// Get source props
		obs_properties_t* props = obs_get_source_properties(sourceId);

		if (props != NULL)
		{
			// Set first available video_device_id value
			obs_property_t* prop_video_device_id = obs_properties_get(props, VIDEO_DEVICE_ID);

			size_t count_video_device_id = obs_property_list_item_count(prop_video_device_id);
			if (count_video_device_id > 0)
			{
#ifdef _WIN32
				const size_t idx = 0;
#else
				const size_t idx = count_video_device_id - 1;
#endif
				obs_data_set_string(
					settings,
					VIDEO_DEVICE_ID,
					obs_property_list_item_string(prop_video_device_id, idx));
			}

			// Will be filled by ObsAddSource
			obs_source_t* source = NULL;
			obs_sceneitem_t* sceneitem = NULL;

			// Create source with default video_device_id
			ObsAddSource(parentScene, sourceId, name, settings, NULL, true, &source, &sceneitem);

			// Wait for dimensions
			for (int i = 0; i < 50 && obs_source_get_width(source) == 0; ++i)
				os_sleep_ms(100);

			size_t src_width = obs_source_get_width(source);
			size_t src_height = obs_source_get_height(source);

			vec2 pos = {};
			pos.x = x;
			pos.y = y;

			vec2 scale = {};
			scale.x = 1;
			scale.y = 1;

			if (maxWidth > 0 && src_width > 0 && maxWidth != src_width)
				scale.x = (float)maxWidth / (float)src_width;

			if (maxHeight > 0 && src_height > 0 && maxHeight != src_height)
				scale.y = (float)maxHeight / (float)src_height;

			if (scale.x != scale.y)
			{
				// Correct aspect ratio
				scale.x = min(scale.x, scale.y);
				scale.y = min(scale.x, scale.y);

				int r_width = (int)((float)src_width * scale.x);
				int r_height = (int)((float)src_height * scale.y);

				pos.x = pos.x + (maxWidth / 2) - (r_width / 2);
				pos.y = pos.y + (maxHeight / 2) - (r_height / 2);
			}

			obs_sceneitem_set_pos(sceneitem, &pos);
			obs_sceneitem_set_scale(sceneitem, &scale);

			// Release references
			obs_sceneitem_release(sceneitem);
			obs_source_release(source);

			// Destroy source props
			obs_properties_destroy(props);
		}
	}

	obs_data_release(settings);
}

void WCUIBrowserDialog::ObsAddSourceGame(
	obs_source_t* parentScene,
	const char* name,
	bool allowTransparency,
	bool limitFramerate,
	bool captureCursor,
	bool antiCheatHook,
	bool captureOverlays)
{
	const char* sourceId = "game_capture";

	// Get default settings
	obs_data_t* settings = obs_get_source_defaults(sourceId);

	// Override default settings
	obs_data_set_bool(settings, "allow_transparency", allowTransparency);
	obs_data_set_bool(settings, "limit_framerate", limitFramerate);
	obs_data_set_bool(settings, "capture_cursor", captureCursor);
	obs_data_set_bool(settings, "anti_cheat_hook", antiCheatHook);
	obs_data_set_bool(settings, "capture_overlays", captureOverlays);

	// Add game capture source
	ObsAddSource(parentScene, sourceId, name, settings, NULL, false);

	// Release ref
	obs_data_release(settings);
}

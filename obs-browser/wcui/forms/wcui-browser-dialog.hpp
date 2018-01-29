#ifndef WCUI_BROWSER_DIALOG_H
#define WCUI_BROWSER_DIALOG_H

#include <obs-frontend-api.h>

#include <QDialog>
#include <QWidget>

#include <util/platform.h>
#include <util/threading.h>
#include <include/cef_version.h>
#include <include/cef_app.h>
#include <include/cef_task.h>

#include <pthread.h>

#include <functional>

// TODO: Remove
#include "shared/browser-client.hpp"

namespace Ui {
	class WCUIBrowserDialog;
}

class WCUIBrowserDialog : public QDialog
{
	Q_OBJECT

private:
	const int BROWSER_HANDLE_NONE = -1;

public:
	explicit WCUIBrowserDialog(
		QWidget* parent,
		std::string obs_module_path,
		std::string cache_path);

	~WCUIBrowserDialog();

public:
	void ShowModal();


private Q_SLOTS:
	// void reject();

private:
	static void* InitBrowserThreadEntryPoint(void* arg);
	void InitBrowser();

public:

private:
	std::string m_obs_module_path;
	std::string m_cache_path;

	Ui::WCUIBrowserDialog* ui;

	cef_window_handle_t m_window_handle;
	int m_browser_handle = BROWSER_HANDLE_NONE;

private:
};

#endif // WCUI_BROWSER_DIALOG_H

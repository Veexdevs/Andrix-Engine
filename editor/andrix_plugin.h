/**************************************************************************/
/*  andrix_plugin.h                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             ANDRIX ENGINE                              */
/*                        https://github.com/Veexdevs/Andrix-Engine       */
/**************************************************************************/
/* Copyright (c) 2026-present Andrix Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md).  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "editor/docks/editor_dock.h"
#include "editor/plugins/editor_plugin.h"

class Button;
class CheckButton;
class HTTPRequest;
class Label;
class LineEdit;
class RichTextLabel;

// Built-in Andrix dock: AI chat/code agent (OpenRouter) + GitHub project sync.
class AndrixPlugin : public EditorPlugin {
	GDCLASS(AndrixPlugin, EditorPlugin)

private:
	EditorDock *dock = nullptr;

	// --- AI tab ---
	HTTPRequest *ai_http = nullptr;
	LineEdit *ai_model = nullptr;
	LineEdit *ai_input = nullptr;
	RichTextLabel *ai_log = nullptr;
	LineEdit *ai_path = nullptr;
	Label *ai_status = nullptr;
	Button *ai_send_btn = nullptr;
	Button *ai_apply_btn = nullptr;
	Array ai_history;
	String ai_pending_code;
	bool ai_busy = false;

	// --- GitHub tab ---
	LineEdit *gh_url = nullptr;
	LineEdit *gh_branch = nullptr;
	Button *gh_clone_btn = nullptr;
	Button *gh_pull_btn = nullptr;
	Button *gh_push_btn = nullptr;
	Button *gh_open_btn = nullptr;
	CheckButton *gh_auto_sync = nullptr;
	Label *gh_status = nullptr;
	RichTextLabel *gh_log = nullptr;
	bool gh_busy = false;
	bool gh_pending_push = false;
	String gh_last_clone_target;

	static void _bind_methods();

	void _build_ui();

	// AI.
	void _ai_send_pressed();
	void _ai_text_submitted(const String &p_text);
	void _ai_request_done(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body);
	void _ai_apply_pressed();
	void _ai_append(const String &p_bbcode);
	String _ai_get_key() const;

	// GitHub.
	void _gh_clone_pressed();
	void _gh_pull_pressed();
	void _gh_push_pressed();
	void _gh_open_pressed();
	void _gh_auto_toggled(bool p_toggled);
	void _gh_run(const PackedStringArray &p_args, const String &p_desc, bool p_is_clone = false);
	void _gh_task(PackedStringArray p_args, String p_desc, bool p_is_clone);
	void _gh_sync_task(String p_dir);
	void _gh_done(String p_desc, int p_exit, String p_out, bool p_is_clone);
	void _gh_sync_done(String p_out, int p_exit);
	void _gh_append(const String &p_bbcode);
	void _gh_maybe_push();
	void _gh_set_busy(bool p_busy);
	void _on_resource_saved(Object *p_resource);
	void _on_scene_saved(const String &p_path);
	String _gh_project_dir() const;
	bool _gh_is_repo() const;

public:
	AndrixPlugin();
	~AndrixPlugin();
};

/**************************************************************************/
/*  andrix_plugin.cpp                                                      */
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

#include "andrix_plugin.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/http_client.h"
#include "core/io/json.h"
#include "core/object/callable_mp.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/editor_interface.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/tab_container.h"
#include "scene/main/http_request.h"
#include "scene/main/scene_tree.h"

static String _andrix_esc(const String &p_text) {
	// Escape BBCode brackets so raw content renders as plain text.
	return p_text.replace("[", "[lb]");
}

void AndrixPlugin::_bind_methods() {
	// No binds required so far.
}

// ---------------------------------------------------------------------------
// Settings helpers
// ---------------------------------------------------------------------------

String AndrixPlugin::_ai_get_key() const {
	EditorSettings *es = EditorSettings::get_singleton();
	if (es != nullptr && es->has_setting("andrix_ai/api_key")) {
		String key = es->get_setting("andrix_ai/api_key");
		if (!key.strip_edges().is_empty()) {
			return key.strip_edges();
		}
	}
	return OS::get_singleton()->get_environment("OPENROUTER_API_KEY").strip_edges();
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

void AndrixPlugin::_build_ui() {
	VBoxContainer *root = memnew(VBoxContainer);
	dock->add_child(root);

	TabContainer *tabs = memnew(TabContainer);
	tabs->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	tabs->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_child(tabs);

	// ---------------- AI tab ----------------
	VBoxContainer *ai_vb = memnew(VBoxContainer);
	ai_vb->set_name(TTRC("IA"));
	tabs->add_child(ai_vb);

	Label *ai_title = memnew(Label);
	ai_title->set_text(TTRC("Agente de IA (OpenRouter) — converse e gere código"));
	ai_vb->add_child(ai_title);

	HBoxContainer *model_row = memnew(HBoxContainer);
	ai_vb->add_child(model_row);
	Label *model_lbl = memnew(Label);
	model_lbl->set_text(TTRC("Modelo:"));
	model_row->add_child(model_lbl);
	ai_model = memnew(LineEdit);
	ai_model->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	ai_model->set_placeholder(TTRC("openai/gpt-5-mini"));
	model_row->add_child(ai_model);

	ai_status = memnew(Label);
	ai_status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	ai_status->set_text(TTRC("Chave: configurada em andrix_ai/api_key ou OPENROUTER_API_KEY."));
	ai_vb->add_child(ai_status);

	ai_log = memnew(RichTextLabel);
	ai_log->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	ai_log->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	ai_log->set_custom_minimum_size(Size2(0, 160));
	ai_log->set_scroll_follow(true);
	ai_log->append_text(TTRC("[color=#9aa0b0]Bem-vindo ao Andrix AI. Pedi um jogo, uma mecânica ou um script e clique em Enviar.[/color]\n"));
	ai_vb->add_child(ai_log);

	HBoxContainer *ai_send_row = memnew(HBoxContainer);
	ai_vb->add_child(ai_send_row);
	ai_input = memnew(LineEdit);
	ai_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	ai_input->set_placeholder(TTRC("Ex.: crie um player que pula duplo..."));
	ai_send_row->add_child(ai_input);
	ai_send_btn = memnew(Button);
	ai_send_btn->set_text(TTRC("Enviar"));
	ai_send_row->add_child(ai_send_btn);

	HBoxContainer *ai_apply_row = memnew(HBoxContainer);
	ai_vb->add_child(ai_apply_row);
	ai_path = memnew(LineEdit);
	ai_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	ai_path->set_text("res://andrix_snippet.gd");
	ai_path->set_placeholder(TTRC("res://caminho/do/script.gd"));
	ai_apply_row->add_child(ai_path);
	ai_apply_btn = memnew(Button);
	ai_apply_btn->set_text(TTRC("Aplicar código"));
	ai_apply_btn->set_disabled(true);
	ai_apply_row->add_child(ai_apply_btn);

	ai_send_btn->connect("pressed", callable_mp(this, &AndrixPlugin::_ai_send_pressed));
	ai_input->connect("text_submitted", callable_mp(this, &AndrixPlugin::_ai_text_submitted));
	ai_apply_btn->connect("pressed", callable_mp(this, &AndrixPlugin::_ai_apply_pressed));

	// ---------------- GitHub tab ----------------
	VBoxContainer *gh_vb = memnew(VBoxContainer);
	gh_vb->set_name(TTRC("GitHub"));
	tabs->add_child(gh_vb);

	Label *gh_title = memnew(Label);
	gh_title->set_text(TTRC("Projeto do GitHub — clone sem escolher pastas e sincronize ao salvar"));
	gh_vb->add_child(gh_title);

	gh_status = memnew(Label);
	gh_status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	gh_status->set_text(TTRC("Cole a URL de um repositório para importar."));
	gh_vb->add_child(gh_status);

	HBoxContainer *gh_url_row = memnew(HBoxContainer);
	gh_vb->add_child(gh_url_row);
	gh_url = memnew(LineEdit);
	gh_url->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	gh_url->set_placeholder(TTRC("https://github.com/usuario/projeto.git"));
	gh_url_row->add_child(gh_url);
	gh_branch = memnew(LineEdit);
	gh_branch->set_custom_minimum_size(Size2(140, 0));
	gh_branch->set_placeholder(TTRC("branch"));
	gh_url_row->add_child(gh_branch);
	gh_clone_btn = memnew(Button);
	gh_clone_btn->set_text(TTRC("Importar"));
	gh_url_row->add_child(gh_clone_btn);

	HBoxContainer *gh_actions_row = memnew(HBoxContainer);
	gh_vb->add_child(gh_actions_row);
	gh_pull_btn = memnew(Button);
	gh_pull_btn->set_text(TTRC("Puxar"));
	gh_actions_row->add_child(gh_pull_btn);
	gh_push_btn = memnew(Button);
	gh_push_btn->set_text(TTRC("Empurrar"));
	gh_actions_row->add_child(gh_push_btn);
	gh_auto_sync = memnew(CheckButton);
	gh_auto_sync->set_text(TTRC("Auto-sync ao salvar"));
	gh_actions_row->add_child(gh_auto_sync);
	gh_open_btn = memnew(Button);
	gh_open_btn->set_text(TTRC("Abrir clonado"));
	gh_open_btn->set_visible(false);
	gh_actions_row->add_child(gh_open_btn);

	gh_log = memnew(RichTextLabel);
	gh_log->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	gh_log->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	gh_log->set_custom_minimum_size(Size2(0, 140));
	gh_log->set_scroll_follow(true);
	gh_vb->add_child(gh_log);

	gh_clone_btn->connect("pressed", callable_mp(this, &AndrixPlugin::_gh_clone_pressed));
	gh_pull_btn->connect("pressed", callable_mp(this, &AndrixPlugin::_gh_pull_pressed));
	gh_push_btn->connect("pressed", callable_mp(this, &AndrixPlugin::_gh_push_pressed));
	gh_open_btn->connect("pressed", callable_mp(this, &AndrixPlugin::_gh_open_pressed));
	gh_auto_sync->connect("toggled", callable_mp(this, &AndrixPlugin::_gh_auto_toggled));
}

// ---------------------------------------------------------------------------
// AI (OpenRouter chat + code apply)
// ---------------------------------------------------------------------------

void AndrixPlugin::_ai_append(const String &p_bbcode) {
	ai_log->append_text(p_bbcode + "\n");
}

void AndrixPlugin::_ai_send_pressed() {
	if (ai_busy) {
		return;
	}
	String text = ai_input->get_text().strip_edges();
	if (text.is_empty()) {
		return;
	}
	String key = _ai_get_key();
	if (key.is_empty()) {
		_ai_append(TTRC("[color=#ff8a8a]Nenhuma chave da API. Defina andrix_ai/api_key nas Configurações do Editor ou a variável de ambiente OPENROUTER_API_KEY.[/color]"));
		return;
	}

	EditorSettings *es = EditorSettings::get_singleton();
	String model = ai_model->get_text().strip_edges();
	if (model.is_empty()) {
		model = "openai/gpt-5-mini";
	}
	if (es != nullptr) {
		es->set_setting("andrix_ai/model", model);
	}

	Array messages;
	Dictionary system_msg;
	system_msg["role"] = "system";
	system_msg["content"] = TTR("Você é o Andrix AI, o agente de código do editor Andrix Engine. Responda em português do Brasil. Quando gerar código, use blocos ``` com a linguagem e o caminho sugerido entre aspas, por exemplo ```gdscript res://player.gd.");
	messages.push_back(system_msg);
	for (int i = 0; i < ai_history.size(); i++) {
		messages.push_back(ai_history[i]);
	}
	Dictionary user_msg;
	user_msg["role"] = "user";
	user_msg["content"] = text;
	messages.push_back(user_msg);
	ai_history.push_back(user_msg);
	while (ai_history.size() > 40) {
		ai_history.remove_at(0);
	}

	Dictionary body;
	body["model"] = model;
	body["messages"] = messages;

	Vector<String> headers;
	headers.push_back("Content-Type: application/json");
	headers.push_back("Authorization: Bearer " + key);

	ai_busy = true;
	ai_send_btn->set_disabled(true);
	ai_status->set_text(TTRC("Pensando..."));

	ai_input->clear();
	_ai_append("[color=#8ab4f8][b]Você:[/b][/color] " + _andrix_esc(text));

	ai_http->set_timeout(90.0);
	Error err = ai_http->request("https://openrouter.ai/api/v1/chat/completions", headers, HTTPClient::METHOD_POST, JSON::stringify(body));
	if (err != OK) {
		ai_busy = false;
		ai_send_btn->set_disabled(false);
		_ai_append(TTRC("[color=#ff8a8a]Falha ao iniciar a requisição HTTP.[/color]"));
	}
}

void AndrixPlugin::_ai_text_submitted(const String &p_text) {
	_ai_send_pressed();
}

void AndrixPlugin::_ai_request_done(int p_result, int p_code, const PackedStringArray &p_headers, const PackedByteArray &p_body) {
	ai_busy = false;
	ai_send_btn->set_disabled(false);

	if (p_result != HTTPRequest::RESULT_SUCCESS) {
		ai_status->set_text(TTRC("Falha de rede."));
		_ai_append(TTRC("[color=#ff8a8a]Falha de rede ao falar com o OpenRouter.[/color]"));
		return;
	}

	String raw = String::utf8((const char *)p_body.ptr(), (int)p_body.size());
	Variant parsed = JSON::parse_string(raw);
	if (parsed.get_type() != Variant::DICTIONARY) {
		ai_status->set_text(TTRC("Resposta inválida."));
		_ai_append(TTRC("[color=#ff8a8a]Resposta inválida do servidor:[/color] ") + _andrix_esc(raw.substr(0, 400)));
		return;
	}
	Dictionary resp = parsed;
	if (resp.has("error")) {
		Dictionary err = resp["error"];
		String msg = err.get("message", JSON::stringify(err));
		ai_status->set_text(TTRC("Erro da API."));
		_ai_append("[color=#ff8a8a]Erro:[/color] " + _andrix_esc(msg));
		return;
	}
	if (p_code != 200 || !resp.has("choices")) {
		ai_status->set_text(vformat(TTRC("HTTP %d"), p_code));
		_ai_append(TTRC("[color=#ff8a8a]Resposta inesperada:[/color] ") + _andrix_esc(raw.substr(0, 400)));
		return;
	}

	Array choices = resp["choices"];
	if (choices.is_empty()) {
		ai_status->set_text(TTRC("Resposta vazia."));
		return;
	}
	Dictionary first = choices[0];
	Dictionary msg_dict = first["message"];
	String content = msg_dict.get("content", "");

	ai_status->set_text(TTRC("Pronto."));
	_ai_append("[color=#7dd3a0][b]IA:[/b][/color] " + _andrix_esc(content));

	Dictionary assistant_msg;
	assistant_msg["role"] = "assistant";
	assistant_msg["content"] = content;
	ai_history.push_back(assistant_msg);

	// Detect a fenced code block and offer to apply it.
	int f1 = content.find("```");
	if (f1 >= 0) {
		int f2 = content.find("```", f1 + 3);
		if (f2 > f1) {
			String block = content.substr(f1 + 3, f2 - f1 - 3);
			int nl = block.find("\n");
			if (nl >= 0) {
				String first_line = block.substr(0, nl).strip_edges();
				if (first_line.length() < 32 && first_line.find(" ") == -1) {
					block = block.substr(nl + 1);
				}
			}
			ai_pending_code = block.strip_edges();
			if (!ai_pending_code.is_empty()) {
				ai_apply_btn->set_disabled(false);
				ai_status->set_text(TTRC("Código detectado — edite o caminho e clique em Aplicar código."));
			}
		}
	}
}

void AndrixPlugin::_ai_apply_pressed() {
	if (ai_pending_code.is_empty()) {
		_ai_append(TTRC("[color=#ffcc66]Nenhum código pendente. Peça um script à IA primeiro.[/color]"));
		return;
	}
	String path = ai_path->get_text().strip_edges();
	if (path.is_empty()) {
		path = "res://andrix_snippet.gd";
	}
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_null()) {
		_ai_append(TTRC("[color=#ff8a8a]Não foi possível abrir o arquivo:[/color] ") + _andrix_esc(path));
		return;
	}
	file->store_string(ai_pending_code + "\n");
	_ai_append(vformat(TTRC("[color=#7dd3a0]Código salvo em %s[/color]"), _andrix_esc(path)));
	EditorFileSystem::get_singleton()->scan();
}

// ---------------------------------------------------------------------------
// GitHub
// ---------------------------------------------------------------------------

String AndrixPlugin::_gh_project_dir() const {
	String path = ProjectSettings::get_singleton()->globalize_path("res://");
	while (path.ends_with("/") || path.ends_with("\\")) {
		path = path.substr(0, path.length() - 1);
	}
	return path;
}

bool AndrixPlugin::_gh_is_repo() const {
	String dir = _gh_project_dir();
	return FileAccess::exists(dir + "/.git/config") || DirAccess::dir_exists_absolute(dir + "/.git");
}

void AndrixPlugin::_gh_append(const String &p_bbcode) {
	gh_log->append_text(p_bbcode + "\n");
}

void AndrixPlugin::_gh_set_busy(bool p_busy) {
	gh_clone_btn->set_disabled(p_busy);
	gh_pull_btn->set_disabled(p_busy);
	gh_push_btn->set_disabled(p_busy);
}

void AndrixPlugin::_gh_run(const PackedStringArray &p_args, const String &p_desc, bool p_is_clone) {
	if (gh_busy) {
		_gh_append(TTRC("[color=#ffcc66]O Git está ocupado. Aguarde a operação atual.[/color]"));
		return;
	}
	if (p_args.is_empty()) {
		return;
	}
	gh_busy = true;
	_gh_set_busy(true);
	gh_status->set_text(TTRC("Trabalhando com o Git..."));
	_gh_append("[b]$ git " + _andrix_esc(String(" ").join(p_args)) + "[/b]");
	WorkerThreadPool::get_singleton()->add_task(callable_mp(this, &AndrixPlugin::_gh_task).bind(p_args, p_desc, p_is_clone), false, "Andrix Git " + p_desc);
}

void AndrixPlugin::_gh_task(PackedStringArray p_args, String p_desc, bool p_is_clone) {
	List<String> args;
	for (int i = 0; i < p_args.size(); i++) {
		args.push_back(p_args[i]);
	}
	String out;
	int exit_code = -1;
	OS::get_singleton()->execute("git", args, &out, &exit_code, true);
	callable_mp(this, &AndrixPlugin::_gh_done).call_deferred(p_desc, exit_code, out, p_is_clone);
}

void AndrixPlugin::_gh_done(String p_desc, int p_exit, String p_out, bool p_is_clone) {
	gh_busy = false;
	_gh_set_busy(false);

	String trimmed = p_out;
	if (trimmed.length() > 6000) {
		trimmed = "...(início cortado)...\n" + trimmed.substr(trimmed.length() - 6000);
	}
	if (trimmed.strip_edges().is_empty()) {
		trimmed = "(sem saída)";
	}
	_gh_append(_andrix_esc(trimmed));
	if (p_exit == 0) {
		_gh_append(TTRC("[color=#7dd3a0]Concluído.[/color]"));
	} else {
		_gh_append(vformat(TTRC("[color=#ff8a8a]Falha (código %d).[/color]"), p_exit));
	}

	if (p_exit == 0 && p_desc == "clone" && !gh_last_clone_target.is_empty()) {
		gh_status->set_text(vformat(TTRC("Projeto clonado em: %s"), gh_last_clone_target));
		gh_open_btn->set_visible(true);
		EditorFileSystem::get_singleton()->scan();
	} else if (p_exit == 0 && p_desc == "pull") {
		gh_status->set_text(TTRC("Projeto atualizado do GitHub."));
		EditorFileSystem::get_singleton()->scan();
	} else if (p_exit == 0 && p_desc == "push") {
		gh_status->set_text(TTRC("Alterações enviadas ao GitHub."));
	} else if (p_exit != 0) {
		gh_status->set_text(TTRC("Operação do Git falhou — veja o log."));
	}

	if (gh_pending_push) {
		gh_pending_push = false;
		_gh_maybe_push();
	}
}

void AndrixPlugin::_gh_clone_pressed() {
	String url = gh_url->get_text().strip_edges();
	if (url.is_empty()) {
		gh_status->set_text(TTRC("Cole a URL do repositório primeiro."));
		return;
	}
	if (_gh_is_repo()) {
		gh_status->set_text(TTRC("Este projeto já é um repositório — use Puxar/Empurrar."));
		return;
	}

	String name = url;
	int slash = name.rfind("/");
	if (slash >= 0) {
		name = name.substr(slash + 1);
	}
	if (name.ends_with(".git")) {
		name = name.substr(0, name.length() - 4);
	}
	String clean;
	for (int i = 0; i < name.length(); i++) {
		char32_t c = name[i];
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
		clean += ok ? String::chr(c) : String("_");
	}
	if (clean.is_empty() || clean == "." || clean == "..") {
		clean = "projeto";
	}

	String home = OS::get_singleton()->get_environment("HOME");
	if (home.is_empty()) {
		home = OS::get_singleton()->get_environment("USERPROFILE");
	}
	if (home.is_empty()) {
		home = ".";
	}
	gh_last_clone_target = home.path_join("AndrixProjects").path_join(clean);
	if (DirAccess::dir_exists_absolute(gh_last_clone_target)) {
		gh_status->set_text(vformat(TTRC("A pasta já existe: %s"), gh_last_clone_target));
		return;
	}

	PackedStringArray args;
	args.push_back("clone");
	String branch = gh_branch->get_text().strip_edges();
	if (!branch.is_empty()) {
		args.push_back("-b");
		args.push_back(branch);
	}
	args.push_back(url);
	args.push_back(gh_last_clone_target);
	_gh_run(args, "clone", true);
}

void AndrixPlugin::_gh_pull_pressed() {
	if (!_gh_is_repo()) {
		gh_status->set_text(TTRC("Este projeto não é um repositório Git."));
		return;
	}
	PackedStringArray args;
	args.push_back("-C");
	args.push_back(_gh_project_dir());
	args.push_back("-c");
	args.push_back("http.lowSpeedLimit=1");
	args.push_back("-c");
	args.push_back("http.lowSpeedTime=20");
	args.push_back("pull");
	args.push_back("--ff-only");
	_gh_run(args, "pull");
}

void AndrixPlugin::_gh_push_pressed() {
	if (!_gh_is_repo()) {
		gh_status->set_text(TTRC("Este projeto não é um repositório Git."));
		return;
	}
	if (gh_busy) {
		gh_pending_push = true;
		_gh_append(TTRC("[color=#ffcc66]Git ocupado — sincronização enfileirada.[/color]"));
		return;
	}
	gh_busy = true;
	_gh_set_busy(true);
	gh_status->set_text(TTRC("Sincronizando com o GitHub..."));
	WorkerThreadPool::get_singleton()->add_task(callable_mp(this, &AndrixPlugin::_gh_sync_task).bind(_gh_project_dir()), false, "Andrix Sync");
}

void AndrixPlugin::_gh_maybe_push() {
	if (gh_auto_sync == nullptr || !gh_auto_sync->is_pressed()) {
		return;
	}
	if (!_gh_is_repo()) {
		return;
	}
	if (gh_busy) {
		gh_pending_push = true;
		return;
	}
	_gh_append(TTRC("[color=#9aa0b0]Auto-sync: enviando alterações...[/color]"));
	gh_busy = true;
	_gh_set_busy(true);
	gh_status->set_text(TTRC("Sincronizando com o GitHub..."));
	WorkerThreadPool::get_singleton()->add_task(callable_mp(this, &AndrixPlugin::_gh_sync_task).bind(_gh_project_dir()), false, "Andrix Sync");
}

void AndrixPlugin::_gh_sync_task(String p_dir) {
	String out;
	int last_exit = 0;

	auto run_cmd = [&](const List<String> &p_args, const String &p_label) {
		String cmd_out;
		int cmd_exit = -1;
		OS::get_singleton()->execute("git", p_args, &cmd_out, &cmd_exit, true);
		out += "[b]" + p_label + "[/b]\n" + cmd_out + "\n";
		last_exit = cmd_exit;
	};

	List<String> status_args;
	status_args.push_back("-C");
	status_args.push_back(p_dir);
	status_args.push_back("status");
	status_args.push_back("--porcelain");
	String status_out;
	int status_exit = -1;
	OS::get_singleton()->execute("git", status_args, &status_out, &status_exit, true);

	if (status_exit != 0) {
		out = status_out;
		last_exit = status_exit;
	} else if (status_out.strip_edges().is_empty()) {
		out = "Nada para sincronizar.";
		last_exit = 0;
	} else {
		List<String> add_args;
		add_args.push_back("-C");
		add_args.push_back(p_dir);
		add_args.push_back("add");
		add_args.push_back("-A");
		run_cmd(add_args, "git add -A");

		if (last_exit == 0) {
			String msg = "Sync by Andrix Engine — " + Time::get_singleton()->get_datetime_string_from_system();
			List<String> commit_args;
			commit_args.push_back("-C");
			commit_args.push_back(p_dir);
			commit_args.push_back("commit");
			commit_args.push_back("-m");
			commit_args.push_back(msg);
			run_cmd(commit_args, "git commit");
		}

		if (last_exit == 0) {
			List<String> push_args;
			push_args.push_back("-C");
			push_args.push_back(p_dir);
			push_args.push_back("-c");
			push_args.push_back("http.lowSpeedLimit=1");
			push_args.push_back("-c");
			push_args.push_back("http.lowSpeedTime=20");
			push_args.push_back("push");
			run_cmd(push_args, "git push");
		}
	}

	callable_mp(this, &AndrixPlugin::_gh_sync_done).call_deferred(out, last_exit);
}

void AndrixPlugin::_gh_sync_done(String p_out, int p_exit) {
	gh_busy = false;
	_gh_set_busy(false);

	String trimmed = p_out;
	if (trimmed.length() > 6000) {
		trimmed = "...(início cortado)...\n" + trimmed.substr(trimmed.length() - 6000);
	}
	_gh_append(_andrix_esc(trimmed));
	if (p_exit == 0) {
		_gh_append(TTRC("[color=#7dd3a0]Sincronização concluída.[/color]"));
		gh_status->set_text(TTRC("Sincronizado com o GitHub."));
	} else {
		_gh_append(vformat(TTRC("[color=#ff8a8a]Falha na sincronização (código %d).[/color]"), p_exit));
		gh_status->set_text(TTRC("Falha na sincronização — veja o log. Configure git user.name/user.email se o commit falhou."));
	}

	if (gh_pending_push) {
		gh_pending_push = false;
		_gh_maybe_push();
	}
}

void AndrixPlugin::_gh_open_pressed() {
	if (gh_last_clone_target.is_empty() || !DirAccess::dir_exists_absolute(gh_last_clone_target)) {
		gh_status->set_text(TTRC("Nenhum projeto clonado nesta sessão."));
		return;
	}
	List<String> args;
	args.push_back("--path");
	args.push_back(gh_last_clone_target);
	Error err = OS::get_singleton()->create_process(OS::get_singleton()->get_executable_path(), args, nullptr);
	if (err != OK) {
		gh_status->set_text(TTRC("Falha ao abrir o projeto clonado."));
		return;
	}
	get_tree()->quit();
}

void AndrixPlugin::_gh_auto_toggled(bool p_toggled) {
	EditorSettings *es = EditorSettings::get_singleton();
	if (es != nullptr) {
		es->set_setting("andrix_git/auto_sync", p_toggled);
	}
	if (p_toggled) {
		_gh_maybe_push();
	}
}

void AndrixPlugin::_on_resource_saved(Object *p_resource) {
	_gh_maybe_push();
}

void AndrixPlugin::_on_scene_saved(const String &p_path) {
	_gh_maybe_push();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

AndrixPlugin::AndrixPlugin() {
	EditorSettings *es = EditorSettings::get_singleton();
	if (es != nullptr) {
		if (!es->has_setting("andrix_ai/model")) {
			es->set_setting("andrix_ai/model", "openai/gpt-5-mini");
		}
		if (!es->has_setting("andrix_ai/api_key")) {
			es->set_setting("andrix_ai/api_key", "");
		}
		if (!es->has_setting("andrix_git/auto_sync")) {
			es->set_setting("andrix_git/auto_sync", true);
		}
	}

	dock = memnew(EditorDock);
	dock->set_name(TTRC("Andrix"));
	dock->set_layout_key("AndrixDock");
	dock->set_icon_name("AndrixAI");
	dock->set_dock_shortcut(ED_SHORTCUT_AND_COMMAND("docks/open_andrix", TTRC("Open Andrix Dock")));
	dock->set_default_slot(EditorDock::DOCK_SLOT_BOTTOM);
	_build_ui();
	EditorDockManager::get_singleton()->add_dock(dock);

	ai_http = memnew(HTTPRequest);
	dock->add_child(ai_http);
	ai_http->connect("request_completed", callable_mp(this, &AndrixPlugin::_ai_request_done));

	// Model field reflects the stored setting.
	if (es != nullptr) {
		String model = es->get_setting("andrix_ai/model");
		if (!model.is_empty()) {
			ai_model->set_text(model);
		}
		bool auto_sync = es->get_setting("andrix_git/auto_sync");
		gh_auto_sync->set_pressed_no_signal(auto_sync);
	}

	// React to editor saves for auto-sync.
	connect("resource_saved", callable_mp(this, &AndrixPlugin::_on_resource_saved));
	connect("scene_saved", callable_mp(this, &AndrixPlugin::_on_scene_saved));

	// If this project is already a Git repository, pull once on startup (async).
	if (_gh_is_repo()) {
		PackedStringArray args;
		args.push_back("-C");
		args.push_back(_gh_project_dir());
		args.push_back("-c");
		args.push_back("http.lowSpeedLimit=1");
		args.push_back("-c");
		args.push_back("http.lowSpeedTime=20");
		args.push_back("pull");
		args.push_back("--ff-only");
		gh_status->set_text(TTRC("Repositório detectado — atualizando do GitHub..."));
		_gh_run(args, "pull");
	}
}

AndrixPlugin::~AndrixPlugin() {
	if (dock != nullptr) {
		memdelete(dock);
		dock = nullptr;
	}
}

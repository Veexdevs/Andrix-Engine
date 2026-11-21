# Andrix Engine

**Andrix Engine** é um fork do [Godot Engine](https://github.com/godotengine/godot) **4.7.2-stable**, com CI no GitHub Actions que compila automaticamente os binários de **Android (`.apk`)**, **Windows (`.exe`)** e **iOS (`.ipa`)**.

> Baseado no Godot Engine (licença MIT). Veja o [repositório oficial do Godot](https://github.com/godotengine/godot) e a [documentação](https://docs.godotengine.org/).

---

## 📦 Builds automáticos (GitHub Actions)

O workflow [`andrix-builds.yml`](.github/workflows/andrix-builds.yml) roda a cada push no `main`, em cada Pull Request ou manualmente (**Actions → Andrix Builds → Run workflow**).

| Alvo | Workflow | Artefatos gerados | Conteúdo |
|---|---|---|---|
| 🤖 **Android (APK)** | `android_builds.yml` | `android-editor` | Editor do Godot para Android (`android_editor-*.apk`) |
| | | `android-template-arm64` | Template de exportação de debug (`android_debug.apk`) |
| | | `android-template-release-arm64` | Template de exportação de release (`android_release.apk`) |
| | | `android-editor-horizonos` / `-picoos` | Editores para Horizon OS e PICO OS |
| 🏁 **Windows (EXE)** | `windows_builds.yml` | `windows-editor` | `godot.windows.editor.x86_64.exe` (editor) |
| | | `windows-template` | `godot.windows.template_release.x86_64.exe` (template de exportação) |
| 🍏 **iOS** | `ios_builds.yml` | `ios-template` | `godot_ios.zip` (templates `debug` + `release`, arm64, com MoltenVK) |
| 🍏 **iOS (.ipa)** | `andrix-builds.yml` → `ipa-export` | `ios-ipa` | App de demonstração assinado (`andrix-demo.ipa`) — **requer segredos de assinatura** (abaixo) |

> Também existem workflows prontos (disponíveis via *workflow_dispatch*): `linux_builds.yml`, `macos_builds.yml`, `web_builds.yml` e `static_checks.yml`.

### Baixando os artefatos

```bash
# Lista os runs
gh run list --repo Veexdevs/Andrix-Engine

# Baixa um artefato (ex.: o editor .exe)
gh run download <run-id> -n windows-editor --repo Veexdevs/Andrix-Engine

# Baixa o .ipa
gh run download <run-id> -n ios-ipa --repo Veexdevs/Andrix-Engine
```

Ou pela web: **Actions → o run → seção Artifacts**.

---

## 🍏 Gerando o `.ipa` assinado (iOS)

O `.ipa` precisa ser assinado com certificados da sua conta Apple (gratuita serve — *Apple Development*). Sem os segredos abaixo, o job `ipa-export` é pulado (os templates de iOS continuam sendo gerados normalmente).

### 1. Gere o certificado `.p12`

No macOS: **Acesso-chaves → Certificados → criar/exportar “Apple Development” → exportar como `.p12`** (escolha uma senha).
Ou no [Apple Developer](https://developer.apple.com/account/resources/certificates/list) → *Certificates* → *Apple Development* → baixar `.cer` → importar no Acesso-chaves → exportar `.p12`.

### 2. Gere o Provisioning Profile (Development)

No [Apple Developer → Profiles](https://developer.apple.com/account/resources/profiles/list):

1. **+** → *iOS App Development*;
2. escolha um **App ID explícito** (ex.: `com.veexdevs.andrixdemo` — **não use wildcard `*`**);
3. selecione seu certificado e **seu dispositivo** (UDID);
4. baixe o `.mobileprovision`.

### 3. Codifique em base64

```bash
base64 -i Certificado.p12 | pbcopy            # valor de IOS_CERTIFICATE_P12
base64 -i Perfil.mobileprovision | pbcopy     # valor de IOS_PROVISIONING_PROFILE
```

### 4. Adicione os segredos no repositório

**Settings → Secrets and variables → Actions → New repository secret:**

| Segredo | Valor |
|---|---|
| `IOS_CERTIFICATE_P12` | conteúdo base64 do `.p12` |
| `IOS_CERTIFICATE_PASSWORD` | senha escolhida na exportação do `.p12` |
| `IOS_PROVISIONING_PROFILE` | conteúdo base64 do `.mobileprovision` |

Próximo run: o job **`iOS (.ipa assinado)`** roda, instala certificado + perfil, exporta o projeto em [`demo/`](demo/) e publica o artefato **`ios-ipa`** (`andrix-demo.ipa`).

> O App ID do perfil precisa casar com o bundle do projeto — o CI lê o bundle automaticamente do perfil. Instale o `.ipa` no dispositivo via Xcode (*Window → Devices and Simulators*) ou TestFlight conforme o tipo de perfil.

---

## 🛠️ Como o build funciona

- **Compilação:** [SCons](https://scons.org/) + cache (`actions/cache`) entre runs — os primeiros runs são lentos (~30–90 min), os seguintes bem mais rápidos.
- **Android:** Ubuntu, Java 17, Gradle (`generateGodotEditor` / `generateGodotTemplates`).
- **Windows:** `windows-latest` com MSVC (e variante MinGW).
- **iOS:** `macos-26`, builds `template_release` + `template_debug` (arm64) com `generate_bundle=yes`, que gera o `bin/godot_ios.zip` (projeto Xcode + `libgodot.ios.*.xcframework` + MoltenVK baixado do Vulkan SDK).
- **Exportação do `.ipa`:** editor oficial do Godot 4.7.2 (macOS) + templates gerados por este repositório + seus certificados.

### Estrutura dos workflows

```
.github/
├── workflows/
│   ├── andrix-builds.yml   ← orquestrador (APK · EXE · IPA)
│   ├── android_builds.yml  ← editado: + template_release, secret opcional
│   ├── ios_builds.yml      ← editado: release + debug + MoltenVK + ios.zip
│   ├── windows_builds.yml  ← intacto (oficial do Godot)
│   └── ... (linux, macos, web, visionos, static_checks — oficiais)
└── actions/                ← actions compostos oficiais do Godot
```

---

## ⬆️ Atualizando a versão do Godot

```bash
# ex.: trocar 4.7.2-stable por uma versão nova
git remote add upstream https://github.com/godotengine/godot.git
git fetch --depth=1 upstream --branch <tag>
git checkout FETCH_HEAD -- .
git commit -m "Atualiza Godot para <tag>"
```

Lembre-se de atualizar a versão nos workflows: nome do zip do editor (`andrix-builds.yml`), diretório `export_templates/<versão>` e o `config/features` do `demo/project.godot`.

---

## 📄 Licença

Distribuído sob a licença **MIT** — veja [`LICENSE.txt`](LICENSE.txt) e [`COPYRIGHT.txt`](COPYRIGHT.txt). Godot Engine © Godot contributors.

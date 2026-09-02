# Diretrizes Obrigatórias de Compilação & Commits (Android)

Este repositório gera o APK do **Mario Kart 64 para Android** através do GitHub Actions usando a biblioteca nativa `libSpaghettify.so`.

---

## ⚠️ Regra Obrigatória para Todo Commit com Alterações C/C++

Toda vez que arquivos de código-fonte **C/C++** forem criados ou alterados (incluindo `src/`, `include/`, `libultraship/`, módulos de rede/multiplayer, menus ImGui ou shaders):

### 1. Recompilar a biblioteca nativa localmente
Execute o script de build:
```bash
./build_android.sh
```
*Este script compilará a biblioteca `libSpaghettify.so` com o NDK Clang/Ninja e rodará automaticamente o `llvm-strip` para manter o binário leve e otimizado (~22MB).*

### 2. Verificar se o binário foi atualizado
Confirme que o arquivo `android/app/libs/arm64-v8a/libSpaghettify.so` foi atualizado:
```bash
ls -lh android/app/libs/arm64-v8a/libSpaghettify.so
```

### 3. Fazer o commit incluindo o `.so`
Adicione as alterações de código e o `.so` no mesmo commit:
```bash
git add src/ include/ libultraship/ android/app/libs/arm64-v8a/libSpaghettify.so
git commit -m "feat/fix: descrição das mudanças"
git push origin main
```

---

## ℹ️ Quando NÃO é necessário recompilar o `.so`?

Você **não** precisa rodar o `./build_android.sh` se a alteração envolver **apenas**:
- Arquivos Java/Kotlin em `android/app/src/main/java/`
- Manifest ou recursos Android (`android/app/src/main/res/`)
- Configuração do Gradle (`build.gradle`, `gradle.properties`)
- Documentação Markdown (`README.md`, etc.)
- Workflows do GitHub (`.github/workflows/`)

Nesses casos, o GitHub Actions ou o Gradle usarão diretamente o `libSpaghettify.so` já presente no repositório.

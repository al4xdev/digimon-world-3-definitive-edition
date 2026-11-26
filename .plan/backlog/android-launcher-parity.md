# Android Launcher Parity & Unification Backlog

Prioridades alinhadas após a auditoria Android vs PC (2026-11-26). Fora de escopo: sync em background / `onPause` no Android.

## 1. Extração incremental e cuidadosa de `launcher/app_core.c` + validação de disco (`S_DISC`)
- **Por que o disco ainda precisa de tratamento no Android:** hoje quem valida o disco é o runtime (`psxstack/runtime/disc.c`). No PC o launcher intercepta `port: fatal: disc:` pelo pipe do processo filho e abre `S_DISC`; no Android `dw3_game_main()` roda in-process e `port_fatal` chama `exit(1)`, fechando o app sem aviso.
- **Abordagem gradual (sem big-bang):**
  1. Começar extraindo um núcleo pequeno e testado (`launcher/app_core.c/.h`) só com tabelas canônicas, mutadores puros e helpers compartilhados entre `main.c` e `main_android.c`.
  2. Adicionar uma checagem não-fatal de disco (reaproveitando o critério do runtime / `psxstack`) antes de `dw3_game_main()` no Android para abrir `S_DISC` em vez de dar `exit(1)`.

## 2. Self-test headless do Android no Linux/CI
- Compilar `DW3_ANDROID_STANDALONE_TEST` no Linux com `PLAT_NULL` (mesmo padrão do `--self-test` do PC) para renderizar todas as telas de `main_android.c` nos 6 idiomas e validar chaves `i18n.h` + largura horizontal automaticamente.

## 3. Polimento menor
- `S_CONFLICT_CONFIRM`: confirmação progressiva 3x com `REV #` no Android (evita clique acidental no touch).
- Auto-hide do Floating Pad quando controle Bluetooth/USB estiver conectado (`AUTO / ON / OFF`).
- Remover `"touch_pad"` de `settings.json` (`launcher/cfg.c:604`) já que `tui.state` persiste o valor (elimina warning `unknown key, ignored`).

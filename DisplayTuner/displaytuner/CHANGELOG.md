# Changelog

## 1.1.0

**Fixed: 3DS (Azahar), PSP and other Vulkan programs would not open after using Display Tuner / Color Studio.**
The boot helper (`displaytuner --keep`, and the older `colorstudio --keep`) opened the display before EmulationStation did, which makes it the
display's DRM master, and never let go. Vulkan (`VK_KHR_display`) needs to be the master, so those programs failed with
`[Vulkan] Failed to create KHR_display surface`. The helper now gives the master up as soon as it opens the display. Setting the gamma and colour
tables does not need it (checked on the RG52 Mini). On a kernel that does need it for that commit, the helper borrows it for the instant of the
commit, only if nobody else has it, and gives it back.

- The launcher now updates an older install by itself (new helper, new boot service) the first time you open it, and turns off the old
  `colorstudio.service`, which also fought over the same table with a different setting. Your Color Studio look is copied to `displaytuner.cfg`
  if you have no Display Tuner setting yet. `Display Tuner.sh --update` does only this, without a window.
- `displaytuner --version`.

Tested on the RG52 Mini: update from an old install, running it twice changes nothing, the setting is still applied (exit code 0) without being
master, and a 3DS game opens. Not tested on the RG43H/V or other devices.

## 1.0

First version: gamma, brightness, contrast, saturation and warmth with a reference picture; boot service that re-applies the setting.

---

## Português

## 1.1.0

**Corrigido: jogos de 3DS (Azahar), PSP e outros programas Vulkan não abriam depois de usar o Display Tuner / Color Studio.**
O ajudante de boot (`displaytuner --keep`, e o `colorstudio --keep` antigo) abria a tela antes do EmulationStation, o que o torna o "dono" da tela
(DRM master), e nunca a soltava. O Vulkan (`VK_KHR_display`) precisa ser o dono, então falhava com `[Vulkan] Failed to create KHR_display surface`.
Agora o ajudante solta o controle assim que abre a tela. Gravar as tabelas de gamma e cor não precisa dele (conferido no RG52 Mini). Num kernel
que precise dele para essa gravação, o ajudante pega emprestado só pelo instante da gravação, se ninguém mais o tiver, e devolve.

- O launcher agora atualiza sozinho uma instalação antiga (ajudante novo, serviço novo) na primeira vez que você o abre, e desliga o
  `colorstudio.service` antigo, que também brigava pela mesma tabela com outro ajuste. O visual do Color Studio é copiado para o
  `displaytuner.cfg` se você ainda não tiver um ajuste do Display Tuner. `Display Tuner.sh --update` faz só isso, sem janela.
- `displaytuner --version`.

Testado no RG52 Mini: atualização de uma instalação antiga, rodar duas vezes não muda nada, o ajuste continua sendo aplicado (código 0) sem ser
dono, e um jogo de 3DS abre. Não testado no RG43H/V nem em outros aparelhos.

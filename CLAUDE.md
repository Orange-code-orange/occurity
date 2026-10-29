# CLAUDE.md — Occurity

Guía para Claude Code al trabajar en este repositorio.

## Repositorio

- El repo git está en la carpeta anidada: `occurity-master/occurity-master/` (el `.git` NO está en la raíz del workspace `occurity-master/`).
- Rama de trabajo: `main`.
- Remoto `origin`: `https://github.com/Orange-code-orange/occurity.git`.
- Qt 6 + CMake. Nota: los archivos usan line endings LF; git avisa de conversión a CRLF en Windows (es inofensivo).

## Despliegue a la Raspberry Pi

La Raspberry Pi ejecuta Occurity y tiene su propio clon del repo que actualiza por `git pull`.
**Por eso el despliegue = commit + push a GitHub, y luego la Pi hace pull y recompila.**

### Datos de la Pi

| Dato | Valor |
|------|-------|
| Usuario | `juan` |
| Host mDNS | `occurity.local` (a veces NO resuelve desde WSL) |
| IP (hotspot) | `172.20.10.7` (usar esta si `occurity.local` falla) |
| Contraseña | `guayaba` |
| Repo en la Pi | `~/occurity` |
| Dir runtime (cwd de Occurity) | `~/occurity/release` |
| Script de actualización | `~/actualizar.sh` |

El script `~/actualizar.sh` en la Pi contiene:

```sh
#!/bin/sh
set -e
cd ~/occurity && git pull
cd build && make -j4 && make install
pkill -x Occurity || true
echo "Listo"
```

(Un watchdog relanza Occurity automáticamente tras el `pkill`.)

### Cómo conectarse por SSH (importante)

El SSH nativo de Windows NO acepta la contraseña por script. Hay que usar **WSL (Ubuntu-24.04)** con el truco `SSH_ASKPASS`.
`/tmp/askpass.sh` se borra entre invocaciones de WSL, así que **recréalo en cada comando**:

```bash
wsl -d Ubuntu-24.04 -- bash -c "printf '#!/bin/bash\necho guayaba\n' > /tmp/askpass.sh; chmod +x /tmp/askpass.sh; export SSH_ASKPASS=/tmp/askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0; ssh -o StrictHostKeyChecking=no juan@172.20.10.7 'COMANDO_REMOTO'"
```

### Procedimiento completo

1. **Confirmar que todo está commiteado y pusheado** (desde `occurity-master/occurity-master/`):
   ```bash
   git status --porcelain -uall      # debe estar limpio tras commitear
   git log origin/main..HEAD --oneline   # vacío = todo pusheado
   ```
   Si hay cambios: `git add -A && git commit -m "..." && git push origin main`.
   Terminar los mensajes de commit con:
   `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`

2. **Ejecutar `actualizar.sh` en la Pi** (hace pull + build + restart):
   ```bash
   wsl -d Ubuntu-24.04 -- bash -c "printf '#!/bin/bash\necho guayaba\n' > /tmp/askpass.sh; chmod +x /tmp/askpass.sh; export SSH_ASKPASS=/tmp/askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0; ssh -o StrictHostKeyChecking=no juan@172.20.10.7 '~/actualizar.sh'"
   ```

3. **Verificar**:
   ```bash
   # dentro del ssh: pgrep -ax Occurity   (debe mostrar el proceso)
   #                 cd ~/occurity && git log --oneline -1   (commit esperado)
   ```

### ⚠️ `charts.xml` NO se actualiza con `git pull`

Occurity lee en runtime `~/occurity/release/charts.xml`, que es una copia del usuario — **`make install` solo instala `charts.xml.example`, no sobrescribe `charts.xml`**.
Si un cambio toca la configuración de charts (p. ej. añadir un chart a un `numkey`), además del push hay que actualizar el archivo runtime en la Pi:

```bash
# comparar primero para no pisar personalizaciones del usuario
ssh ... "cd ~/occurity && diff release/charts.xml charts.xml.example"
# si la única diferencia es lo que acabas de añadir, copiar y reiniciar:
ssh ... "cd ~/occurity && cp release/charts.xml release/charts.xml.bak && cp charts.xml.example release/charts.xml && pkill -x Occurity || true"
```

### Problemas conocidos

- Si `git pull` falla por "local changes would be overwritten" en la Pi: suelen ser restos de `scp` manuales de sesiones viejas y ya están en GitHub. Guárdalos con `git stash push -m "..."` (recuperable) y reintenta `actualizar.sh`.
- `occurity.local` no siempre resuelve desde WSL → usar la IP `172.20.10.7`.
- Las rutas relativas en SSH parten de `~` (home), no de `~/occurity`: usa `cd ~/occurity &&` o rutas absolutas.

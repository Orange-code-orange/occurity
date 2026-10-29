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
| Host mDNS | `occurity.local` (NO resuelve desde WSL; desde Windows a veces resuelve primero a IPv6) |
| IP | **VARIABLE** — depende de la red. Resuélvela antes de desplegar con `ping -4 -n 1 occurity.local` desde Windows. Vistas hasta ahora: `172.20.10.7` (hotspot), `192.168.1.66` (LAN) |
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

### Paso 0: resolver la IP de la Pi (la IP cambia según la red)

Desde Windows, forzando IPv4 (mDNS puede devolver primero IPv6, que WSL no puede usar):

```bash
ping -4 -n 1 occurity.local    # muestra la IPv4 actual, p.ej. 192.168.1.66
```

Usa esa IP en todos los comandos SSH siguientes (sustituye `$PI_IP`).

### Cómo conectarse por SSH (importante)

El SSH nativo de Windows NO acepta la contraseña por script. Hay que usar **WSL (Ubuntu-24.04)** con el truco `SSH_ASKPASS`.
`/tmp/askpass.sh` se borra entre invocaciones de WSL, así que **recréalo en cada comando**.
WSL NO resuelve `occurity.local` → siempre usa la IPv4 resuelta en el paso 0:

```bash
wsl -d Ubuntu-24.04 -- bash -c "printf '#!/bin/bash\necho guayaba\n' > /tmp/askpass.sh; chmod +x /tmp/askpass.sh; export SSH_ASKPASS=/tmp/askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0; ssh -o StrictHostKeyChecking=no -o ConnectTimeout=20 juan@$PI_IP 'COMANDO_REMOTO'"
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

2. **Ejecutar `actualizar.sh` en la Pi** (hace pull + build + restart; `$PI_IP` = IP del paso 0):
   ```bash
   wsl -d Ubuntu-24.04 -- bash -c "printf '#!/bin/bash\necho guayaba\n' > /tmp/askpass.sh; chmod +x /tmp/askpass.sh; export SSH_ASKPASS=/tmp/askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0; ssh -o StrictHostKeyChecking=no -o ConnectTimeout=20 juan@$PI_IP '~/actualizar.sh'"
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
- **La IP de la Pi cambia según la red** → resuélvela siempre con `ping -4 -n 1 occurity.local` (paso 0) antes de desplegar. `occurity.local` no resuelve desde WSL y desde Windows puede devolver IPv6 primero (por eso `-4`).
- Timeouts intermitentes en el puerto 22 tras cambiar de red/IP: confirma primero con `ping` desde Windows y WSL, resuelve la IP de nuevo y reintenta.
- Las rutas relativas en SSH parten de `~` (home), no de `~/occurity`: usa `cd ~/occurity &&` o rutas absolutas.

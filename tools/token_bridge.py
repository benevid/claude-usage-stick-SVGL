#!/usr/bin/env python3
"""
token_bridge.py — envia a contagem de tokens das sessoes locais do Claude Code
para o Claude Usage Stick.

Por que existe: a API da Anthropic NAO expoe contagem de tokens para contas de
assinatura (os headers unified-* so trazem utilizacao em %). Quem tem os numeros
reais sao os transcripts locais do Claude Code (~/.claude/projects/**/*.jsonl),
que registram `usage` por mensagem. Este script soma os tokens da janela de 5h
atual (inicio = reset - 5h, obtido do proprio device via GET /window) e faz
POST /tokens no gadget.

Uso:
    python3 tools/token_bridge.py                 # um envio
    python3 tools/token_bridge.py --loop 120      # reenvia a cada 120s
    python3 tools/token_bridge.py --host 192.168.1.50
    python3 tools/token_bridge.py --account Trabalho --loop 120

Agendar no macOS (a cada 2 min, exemplo com cron):
    */2 * * * * /usr/bin/python3 /caminho/token_bridge.py >/dev/null 2>&1

Sem dependencias externas (stdlib).
"""
import argparse
import glob
import json
import os
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone

PROJECTS = os.path.expanduser("~/.claude/projects")


def parse_ts(ts: str) -> float:
    """ISO-8601 (com Z) -> epoch. Retorna 0 se nao parsear."""
    try:
        return datetime.fromisoformat(ts.replace("Z", "+00:00")).timestamp()
    except Exception:
        return 0.0


def collect(start: float):
    """Soma usage de todas as mensagens com timestamp >= start (dedupe por id)."""
    tin = tout = tcache = 0
    seen = set()
    sessions = set()
    for path in glob.glob(os.path.join(PROJECTS, "*", "*.jsonl")):
        try:
            if os.path.getmtime(path) < start - 60:
                continue  # arquivo nao mexido desde antes da janela
            with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                for line in fh:
                    if '"usage"' not in line:
                        continue
                    try:
                        rec = json.loads(line)
                    except Exception:
                        continue
                    msg = rec.get("message") or {}
                    usage = msg.get("usage")
                    if not usage:
                        continue
                    if parse_ts(rec.get("timestamp", "")) < start:
                        continue
                    key = msg.get("id") or rec.get("uuid")
                    if key and key in seen:
                        continue  # mensagens repetidas no transcript
                    if key:
                        seen.add(key)
                    tin += usage.get("input_tokens", 0) or 0
                    tin += usage.get("cache_creation_input_tokens", 0) or 0
                    tout += usage.get("output_tokens", 0) or 0
                    tcache += usage.get("cache_read_input_tokens", 0) or 0
                    sessions.add(path)
        except OSError:
            continue
    return tin, tout, tcache, len(sessions)


def push(host: str, account=None) -> None:
    # janela atual direto do device (h5_reset vem dos headers unified-*)
    with urllib.request.urlopen(f"http://{host}/window", timeout=6) as r:
        win = json.load(r)

    active = win.get("account")
    if account and active and account != active:
        print(f"[bridge] device monitorando '{active}', nao '{account}' — aguardando")
        return

    reset = int(win.get("h5_reset") or 0)
    now = time.time()
    start = (reset - 5 * 3600) if reset > now - 86400 else now - 5 * 3600

    tin, tout, tcache, nsess = collect(start)
    body = {"in": tin, "out": tout, "cache": tcache, "sessions": nsess}
    if account:
        body["account"] = account
    req = urllib.request.Request(
        f"http://{host}/tokens", data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json"}, method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=6) as r:
            r.read()
    except urllib.error.HTTPError as e:
        if e.code == 409:
            print(f"[bridge] device trocou de conta ativa — push ignorado")
            return
        raise
    t0 = datetime.fromtimestamp(start).strftime("%H:%M")
    who = f" [{account}]" if account else ""
    print(f"[bridge]{who} janela desde {t0}: entrada={tin:,} saida={tout:,} "
          f"cache={tcache:,} sessoes={nsess} -> {host} ok")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host", default="claude-stick.local",
                    help="hostname/IP do gadget (default: claude-stick.local)")
    ap.add_argument("--loop", type=int, default=0, metavar="SEG",
                    help="repetir a cada SEG segundos (0 = um envio)")
    ap.add_argument("--account", default=None, metavar="ROTULO",
                    help="rotulo da conta deste host no gadget (multi-conta); "
                         "sem ele o push vale para a conta ativa, seja qual for")
    args = ap.parse_args()
    while True:
        try:
            push(args.host, args.account)
        except Exception as e:
            print(f"[bridge] erro: {e}")
        if not args.loop:
            break
        time.sleep(args.loop)


if __name__ == "__main__":
    main()

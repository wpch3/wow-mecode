#!/usr/bin/env python3
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
CORE=(ROOT/'payload/lua_scripts/extensions/G23Core.ext').read_text(encoding='utf-8')
SERVER=(ROOT/'payload/lua_scripts/custom_server_assistant.lua').read_text(encoding='utf-8')
GM=(ROOT/'payload/lua_scripts/custom_gmhelp.lua').read_text(encoding='utf-8')
WELCOME=(ROOT/'payload/lua_scripts/custom_welcome.lua').read_text(encoding='utf-8')

def need(v,m):
    if not v: raise AssertionError(m)

need('PASS_THROUGH = {}' in CORE and 'if result == G23.PASS_THROUGH then return end' in CORE,
     'explicit native-command pass-through missing')
need('if not player then return end' in CORE, 'console pass-through missing')
need('minRank = tonumber(minRank) or 0' in CORE and 'entry.minRank <= maxRank' in CORE,
     'rank-aware shared help missing')
need('return G23.PASS_THROUGH' in SERVER, 'unknown/native .server subtree is not passed through')
for native in ('info','restart','shutdown','motd','set'):
    need(f'{native} = true' not in SERVER.split('local CUSTOM = {',1)[1].split('}',1)[0],
         f'native .server {native} was accidentally claimed')
need(SERVER.find('G23.CharQuery(')>SERVER.find('local function showDaily'), 'daily DB query is top-level')
need(SERVER.find('G23.WorldQuery(')>SERVER.find('local function showHealth'), 'health DB query is top-level')
need('G23.GetHelpEntries(G23.GetRank(player))' in WELCOME, 'help2 is not rank filtered')

cmds=re.findall(r'E\(\d+,\s*"([^"]+)"',GM)
need(len(cmds)>=150, f'gmhelp catalog too small: {len(cmds)}')
need(len(cmds)==len(set(cmds)), 'gmhelp contains duplicate command entries')
expected={
'.server','.help2','.gmhelp','.luadiag','.bigtest','.tp','.set','.bar','.combo','.buff','.setup',
'.npcbot','.pbot','.bf','.botfind','.tome','.bd','.botdiag','.botname','.pin status',
'.gearset','.add','.add!','.item','.transmog','.spell clean','.reloaditem','.gear',
'.model','.disguise','.morph','.demorph','.findmodel','.fm','.spawn','.spawn!','.clean',
'.protect','.killr','.npcclean','.inst','.raidbuff','.service','.dummy','.nst','.scene','.emote','.say',
'.modify allstats','.modify stat','.tele','.appear','.summon','.recall','.gps','.npc add','.gobject add',
'.server info','.server restart','.server shutdown','.reload config'
}
missing=sorted(expected-set(cmds))
need(not missing, 'current project/core commands missing from gmhelp: '+', '.join(missing))
need('SELECT name,IFNULL(help,\'\') FROM command' in GM, 'world.command dynamic source missing')
need(GM.find('G23.WorldQuery(')>GM.find('local function queryCore'), 'gmhelp core query is top-level')
for token in ('local sessions','SESSION_TTL','sessionOf('):
    need(token not in GM, f'gmhelp reintroduced cross-state session: {token}')
need('RegisterPlayerGossipEvent(MENU_ID, 2, onGossip)' in GM, 'gmhelp gossip not registered')
need('G23.RegisterCommand("gmhelp"' in GM and 'G23.RegisterCommand("server"' in SERVER,
     'P3A commands not registered through shared dispatcher')
print(f'G23P3A_STATIC=PASS catalog={len(cmds)}')

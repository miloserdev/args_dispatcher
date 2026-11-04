#!/usr/bin/env bash
# Test suite for xf (CLI) and xf_repl (REPL).
#   tests/run.sh            run everything
#   tests/run.sh -v         also print output of failing cases
# Cases marked `xfail` document known defects: they are expected to fail
# and are reported as XFAIL; if they start passing they are reported as XPASS.
cd "$(dirname "$0")/.." || exit 2

VERBOSE=0; [ "$1" = "-v" ] && VERBOSE=1

B=build/tests
mkdir -p "$B"
make -s >/dev/null 2>&1 || { echo "build failed"; exit 2; }

CFLAGS_T="-O0 -g -Wall -Wextra -Isrc"
SAN="-fsanitize=address,undefined -fno-omit-frame-pointer"
cc $CFLAGS_T -Itests -o "$B/xf_test" tests/xf_test.c 2>/dev/null || { echo "xf_test build failed"; exit 2; }
HAVE_SAN=0
if cc $CFLAGS_T $SAN -o "$B/xf_asan" src/xf.c 2>/dev/null \
   && cc $CFLAGS_T $SAN -o "$B/xf_repl_asan" src/xf_repl.c -lreadline 2>/dev/null \
   && cc $CFLAGS_T $SAN -Itests -o "$B/xf_test_asan" tests/xf_test.c 2>/dev/null \
   && cc $CFLAGS_T $SAN -o "$B/comp_leak_asan" tests/comp_leak.c -lreadline 2>/dev/null; then HAVE_SAN=1; fi
export ASAN_OPTIONS=detect_leaks=1

XF=build/xf; REPL=build/xf_repl; XT="$B/xf_test"
if [ $HAVE_SAN = 1 ]; then XFS="$B/xf_asan"; REPLS="$B/xf_repl_asan"; XTS="$B/xf_test_asan"
else XFS=$XF; REPLS=$REPL; XTS=$XT; fi

pass=0; fail=0; xfail=0; xpass=0
OUT=""; RC=0

norm() { sed -E 's/0x[0-9a-f]+/<PTR>/g; s/\x1b\[[0-9;]*[A-Za-z]//g; s/\r//g'; }
cli()  { local bin=$1; shift; OUT=$("$bin" "$@" 2>&1); RC=$?; OUT=$(norm <<<"$OUT"); }
# repl BIN line...   (each argument is one input line; \t and \x15 are interpreted)
repl() { local bin=$1; shift; OUT=$(printf '%b\n' "$@" | "$bin" 2>&1); RC=$?; OUT=$(norm <<<"$OUT"); }
# comp BIN keys   - type keys, then discard the line (Ctrl-U, Enter), then EOF
comp() { local bin=$1 keys=$2; OUT=$(printf '%b\x15\n' "$keys" | "$bin" 2>&1); RC=$?; OUT=$(norm <<<"$OUT"); }

begin() { NAME=$1; XFAIL=${2:-}; FAILED=0; MSG=""; }
bad()   { FAILED=1; MSG+=" [$1]"; }
has()   { grep -Eq -- "$1" <<<"$OUT" || bad "missing /$1/"; }
lacks() { grep -Eq -- "$1" <<<"$OUT" && bad "unexpected /$1/"; }
count() { local n; n=$(grep -Ec -- "$1" <<<"$OUT"); [ "$n" -eq "$2" ] || bad "count /$1/ = $n, want $2"; }
rc_is() { [ "$RC" -eq "$1" ] || bad "rc=$RC, want $1"; }
rc_nz() { [ "$RC" -ne 0 ] || bad "rc=0, want non-zero"; }
no_crash() { [ "$RC" -lt 128 ] || bad "crashed rc=$RC"; lacks 'AddressSanitizer|runtime error'; }
before() { # first match of $1 appears on an earlier line than first match of $2
	local a b
	a=$(grep -En -- "$1" <<<"$OUT" | head -1 | cut -d: -f1)
	b=$(grep -En -- "$2" <<<"$OUT" | head -1 | cut -d: -f1)
	{ [ -n "$a" ] && [ -n "$b" ] && [ "$a" -lt "$b" ]; } || bad "order /$1/ before /$2/"
}
end() {
	if [ -z "$XFAIL" ]; then
		if [ $FAILED = 0 ]; then pass=$((pass+1)); printf 'PASS   %s\n' "$NAME"
		else fail=$((fail+1)); printf 'FAIL   %s:%s\n' "$NAME" "$MSG"
			[ $VERBOSE = 1 ] && sed 's/^/         | /' <<<"$OUT"; fi
	else
		if [ $FAILED = 1 ]; then xfail=$((xfail+1)); printf 'XFAIL  %s\n' "$NAME"
		else xpass=$((xpass+1)); printf 'XPASS  %s (known defect seems fixed)\n' "$NAME"; fi
	fi
}
X=xfail

pad() { head -c "$1" < /dev/zero | tr '\0' 'a'; }

echo "== CLI: basics"
begin "no args prints usage and tree, rc=1";            cli $XF; has 'available commands'; has '^  chassis'; rc_is 1; end
begin "power on runs handler, rc=0";                    cli $XF power on; has '\[fn\] power on'; rc_is 0; end
begin "chassis status: parent fn runs before child";    cli $XF chassis status; has 'chassis scan'; has 'chassis status: ON'; before 'chassis scan' 'chassis status: ON'; end
begin "chassis alone: runs fn, then lists subcommands"; cli $XF chassis; has 'chassis scan'; has 'available commands'; has 'identify'; lacks '\[fn\] chassis status'; end
begin "power alone: lists subcommands, no call";        cli $XF power; has 'available commands'; has 'Power on the chassis'; lacks '\[__call\]'; end

echo "== CLI: prefix matching"
begin "prefix: pow on";              cli $XF pow on; has '\[fn\] power on'; end
begin "prefix: p on";                cli $XF p on; has '\[fn\] power on'; end
begin "prefix at both levels: ch bootdev pxe"; cli $XF ch bootdev pxe; has 'chassis bootdev=pxe'; end
begin "ambiguous prefix reports ambiguity";    cli $XF power o; has 'ambiguous command o'; lacks '\[fn\] power'; end
begin "ambiguous prefix: single error message" $X; cli $XF power o; lacks 'unknown command'; end
begin "ambiguous prefix: in s (set/show/splash)"; cli $XF in s; has 'ambiguous command s'; end
begin "ambiguous prefix: single error (in s)" $X; cli $XF in s; lacks 'unknown command'; end
begin "ambiguous root prefix: i (info/ip)";       cli $XF i s; has 'ambiguous command i'; end
begin "matching is case-sensitive (documented)"; cli $XF CHASSIS status; has 'unknown command <CHASSIS>'; end

echo "== CLI: unknown input and exit codes"
begin "unknown root command";        cli $XF xyz; has 'unknown command <xyz>'; end
begin "unknown nested command";      cli $XF power xyz; has 'unknown command <xyz>'; lacks '\[fn\]'; end
begin "empty-string argument does not crash"; cli $XF ""; no_crash; has 'available commands'; end
begin "--help / -h is understood" $X; cli $XF -h; lacks 'unknown command'; end
begin "exit code 0 on success";      cli $XF power on; rc_is 0; end
begin "exit code != 0 on unknown command" $X;  cli $XF xyz; rc_nz; end
begin "exit code != 0 on usage error" $X;      cli $XF chassis identify; rc_nz; end
begin "exit code != 0 on ambiguous command" $X; cli $XF power o; rc_nz; end

echo "== CLI: arguments"
begin "missing argument -> usage";   cli $XF chassis identify; has 'usage: Blink identify'; lacks '\[fn\] chassis identify'; end
begin "argument is passed to handler"; cli $XF chassis identify on; has 'chassis identify=on'; end
begin "two arguments: info set a b"; cli $XF info set a b; has 'info set a=b'; end
begin "too few arguments: info set a"; cli $XF info set a; has 'usage: set <what> <to>'; lacks 'info set a='; end
begin "extra argument is rejected: chassis identify on extra" $X; cli $XF chassis identify on extra; has 'error: .*(extra|too many|unexpected)|usage'; end
begin "extra argument is rejected: power on off" $X;             cli $XF power on off; has 'error: .*(extra|too many|unexpected)|usage'; end
begin "extra argument is rejected: chassis status extra" $X;     cli $XF chassis status extra; has 'error: .*(extra|too many|unexpected)|usage'; end
begin "misconfigured entry (no fn, no next)"; cli $XF chassis asd x; has 'misconfigured'; end

echo "== CLI: chains (self-loop tables)"
begin "info set a b show: both run" $X;  cli $XF info set a b show; has 'info set a=b'; has '\[fn\] info show'; end
begin "info show set a b splash" $X;     cli $XF info show set a b splash; has '\[fn\] info show'; has 'info set a=b'; has '██'; end
begin "info show show runs twice" $X;    cli $XF info show show; count '\[fn\] info show' 2; end
begin "info splash prints banner";       cli $XF info splash; has '██'; end

echo "== CLI: dispatcher semantics (synthetic table)"
begin "exact match wins over longer name";   cli $XT ab; has 'H_AB'; lacks 'H_ABC'; has 'RET=0'; end
begin "longer name reachable";               cli $XT abc; has 'H_ABC'; lacks 'H_AB$'; end
begin "prefix of two names is ambiguous";    cli $XT a; has 'ambiguous'; lacks 'H_AB'; end
begin "unique prefix: abc -> abc, 'ne' -> ambiguous (neg/negchain)"; cli $XT ne; has 'ambiguous'; end
begin "unknown name -> RET=-1";              cli $XT zzz; has 'unknown command'; has 'RET=-1'; end
begin "handler returns <0: error, RET=-1";   cli $XT neg; has 'H_NEG'; has 'RET=-1'; has 'command execution'; end
begin "handler <0 aborts the chain";         cli $XT negchain one; has 'H_NEG'; lacks 'H_OK'; has 'RET=-1'; end
begin "handler returns >0: treated as ok";   cli $XT poschain one; has 'H_POS'; has 'H_OK'; has 'RET=0'; end
begin "chain runs any number of commands";   cli $XT chain one two one; count 'H_OK' 3; has 'RET=0'; end
begin "chain with unknown member stops with error"; cli $XT chain one zzz two; count 'H_OK' 1; has 'RET=-1'; end
begin "31 chained commands (32 tokens) all run"; cli $XT chain $(printf 'one %.0s' $(seq 31)); count 'H_OK' 31; end
begin "33rd+ token is not silently dropped (MAX_TOKENS)" $X; cli $XT chain $(printf 'one %.0s' $(seq 40)); count 'H_OK' 40; end

echo "== CLI: input limits and odd input (ASan/UBSan if available)"
begin "line of exactly 255 chars is accepted";    cli $XFS power on "$(pad 246)"; no_crash; has '\[fn\] power on'; end
begin "line of 256 chars is rejected cleanly";    cli $XFS power on "$(pad 247)"; no_crash; has 'too long'; rc_is 1; end
begin "line of 300 chars is rejected cleanly";    cli $XFS power on "$(pad 300)"; no_crash; has 'too long'; rc_is 1; end
begin "very long command name";                   cli $XFS "$(pad 4000)"; no_crash; rc_is 1; end
begin "many arguments (1000)";                    cli $XFS $(printf 'x %.0s' $(seq 1000)); no_crash; end
begin "tab inside a single argument splits tokens"; cli $XFS $'power\ton'; no_crash; has '\[fn\] power on'; end
begin "trailing tab/space in argument";           cli $XFS power $'on\t '; no_crash; has '\[fn\] power on'; end
begin "unicode argument";                         cli $XFS power $'\xd1\x8b\xd1\x8b'; no_crash; has 'unknown command'; end
begin "quoted argument with a space stays one token" $X; cli $XF info set "a b" c; has 'info set a b=c'; end
begin "only-whitespace argument";                 cli $XFS "   "; no_crash; end
begin "dispatcher does not overflow on 32 tokens"; cli $XTS chain $(printf 'one %.0s' $(seq 31)); no_crash; end

echo "== CLI: ip (keyword/value parsing)"
ipc() { cli $XF ip "$@"; }
begin "addr add: address and device";        ipc addr add 10.10.10.10/24 dev eth0; has '\[ip addr\] add address 10.10.10.10/24 \(ipv4, /24\) to device eth0'; end
begin "addr add without plen = host";        ipc addr add 10.10.10.10 dev eth0; has '/32\) to device eth0'; end
begin "addr add ipv6";                       ipc addr add 2001:db8::1/64 dev eth0; has 'ipv6, /64'; end
begin "addr add with label and broadcast";   ipc addr add 10.10.10.10/24 dev eth0 label eth0:1 broadcast 10.10.10.255; has 'label eth0:1, broadcast 10.10.10.255'; end
begin "addr add keyword order is free";      ipc addr add 10.10.10.10/24 label x dev eth0; has 'to device eth0, label x'; end
begin "addr del";                            ipc addr del 10.10.10.10/24 dev eth0; has '\[ip addr\] delete address 10.10.10.10/24 .* from device eth0'; end
begin "addr del: label not allowed";         ipc addr del 10.10.10.10/24 dev eth0 label x; has 'unknown keyword <label>'; end
begin "addr show (all)";                     ipc addr show; has 'show addresses of all devices'; end
begin "addr show dev";                       ipc addr show dev eth0; has 'show addresses of device eth0'; end
begin "addr flush";                          ipc addr flush dev eth0; has 'flush all addresses of device eth0'; end
begin "addr flush needs dev";                ipc addr flush; has 'missing required keyword <dev>'; end
begin "addr add: abbreviations a a";         cli $XF ip a a 10.0.0.1/24 dev eth0; has '\[ip addr\] add address'; end
begin "addr add: missing address -> usage";  ipc addr add; has 'usage: add <addr>'; end
begin "addr add: missing dev";               ipc addr add 10.10.10.10/24; has 'missing required keyword <dev>'; has 'command execution'; end
begin "addr add: dev without value";         ipc addr add 10.10.10.10/24 dev; has 'keyword <dev> needs a value'; end
begin "addr add: duplicated keyword";        ipc addr add 10.10.10.10/24 dev eth0 dev eth1; has 'given twice'; end
begin "addr add: unknown keyword";           ipc addr add 10.10.10.10/24 dev eth0 foo bar; has 'unknown keyword <foo>'; end
begin "addr add: bad plen /33";              ipc addr add 10.10.10.10/33 dev eth0; has 'invalid address'; lacks '\[ip addr\]'; end
begin "addr add: bad octet";                 ipc addr add 10.10.10.300/24 dev eth0; has 'invalid address'; end
begin "addr add: empty plen";                ipc addr add 10.10.10.10/ dev eth0; has 'invalid address'; end
begin "addr add: not an address";            ipc addr add hello dev eth0; has 'invalid address'; end
begin "addr add: device name too long";      ipc addr add 10.10.10.10/24 dev averyveryverylongname; has 'invalid device name'; end
begin "addr add: broadcast family mismatch"; ipc addr add 10.10.10.10/24 dev eth0 broadcast ::1; has 'invalid broadcast'; end
begin "addr add: keyword abbreviation (the reported case)"; cli $XF ip a a 10.10.10.1/24 d eth0; has 'add address 10.10.10.1/24 .* to device eth0'; lacks 'unknown keyword'; end
begin "keyword abbreviation: de -> dev";    ipc addr add 10.10.10.1/24 de eth0; has 'to device eth0'; end
begin "keyword abbreviation: several at once"; ipc route add 10.1.1.0/24 v 10.10.10.254 d eth0 m 5 o; has 'via 10.10.10.254 dev eth0 metric 5 onlink'; end
begin "keyword abbreviation: flag (u -> up)"; ipc link set eth0 u; has 'state up'; end
begin "keyword abbreviation: label l";      ipc addr add 10.10.10.1/24 d eth0 l x; has 'label x'; end
begin "keyword abbreviation: dev given twice (d + dev)"; ipc addr add 10.10.10.1/24 d eth0 dev eth1; has 'given twice'; end
begin "keyword abbreviation: unknown stays unknown"; ipc addr add 10.10.10.1/24 d eth0 x y; has 'unknown keyword <x>'; end
begin "keyword abbreviation: not a prefix (devx)"; ipc addr add 10.10.10.1/24 devx eth0; has 'unknown keyword <devx>'; end
begin "keyword abbreviation: empty token is not a prefix"; cli $XF ip addr add 10.10.10.1/24 "" eth0; has 'unknown keyword'; end
begin "keyword abbreviation: valid value is not re-parsed (value 'd')"; ipc addr add 10.10.10.1/24 dev d; has 'to device d'; end
begin "keyword abbreviation: ambiguous prefix s (src/scope)"; cli $XT kw s x; has 'ambiguous keyword <s>'; has 'RET=-1'; end
begin "keyword abbreviation: sc -> scope";  cli $XT kw sc host; has 'KW scope=host'; end
begin "keyword abbreviation: src exact";    cli $XT kw src 1.1.1.1; has 'KW src=1.1.1.1'; end
begin "keyword abbreviation: exact beats longer name (dev vs device)"; cli $XT kw dev a; has 'KW dev=a'; lacks 'KW device'; end
begin "keyword abbreviation: device exact"; cli $XT kw device a; has 'KW device=a'; end
begin "keyword abbreviation: dev prefix of device is ambiguous when not exact"; cli $XT kw de a; has 'ambiguous keyword <de>'; end
begin "keyword abbreviation: flag and value mix"; cli $XT kw u sc host; has 'KW up='; has 'KW scope=host'; end
begin "route add default via dev";           ipc route add default via 10.10.10.254 dev eth0; has '\[ip route\] add default route \(ipv4\) via 10.10.10.254 dev eth0'; end
begin "route add host route";                ipc route add 10.10.10.1 dev eth0; has 'add route to host 10.10.10.1 \(ipv4\) dev eth0'; lacks 'via'; end
begin "route add network with all options";  ipc route add 10.20.0.0/16 via 10.10.10.254 dev eth0 src 10.10.10.10 metric 100 onlink; has 'network 10.20.0.0/16 .* via 10.10.10.254 dev eth0 src 10.10.10.10 metric 100 onlink'; end
begin "route add via only (no dev)";         ipc route add 10.20.0.0/16 via 10.10.10.254; has 'via 10.10.10.254'; lacks ' dev '; end
begin "route add ipv6 default";              ipc route add default via fe80::1 dev eth0; has 'default route \(ipv6\) via fe80::1'; end
begin "route add ipv6 network";              ipc route add 2001:db8::/32 via fe80::1 dev eth0; has 'network 2001:db8::/32 \(ipv6\)'; end
begin "route del";                           ipc route del 10.20.0.0/16 via 10.10.10.254 dev eth0; has '\[ip route\] delete route to network'; end
begin "route replace";                       ipc route replace default via 10.10.10.253 dev eth0 metric 50; has '\[ip route\] replace default route .* metric 50'; end
begin "route show";                          ipc route show; has 'show all routes'; end
begin "route show dev";                      ipc route show dev eth0; has 'show routes of device eth0'; end
begin "route get";                           ipc route get 8.8.8.8; has 'look up the route to 8.8.8.8'; end
begin "route get: bad address";              ipc route get 8.8.8; has 'invalid address'; end
begin "route get: prefix not allowed";       ipc route get 8.8.8.0/24; has 'invalid address'; end
begin "route get: extra tokens rejected";    ipc route get 8.8.8.8 dev eth0; has 'unknown keyword <dev>'; end
begin "route add: needs via or dev";         ipc route add 10.1.1.0/24; has 'needs at least <via> or <dev>'; end
begin "route add: bad destination";          ipc route add 10.1.1.0/40 dev eth0; has 'invalid destination'; end
begin "route add: bad gateway";              ipc route add default via 10.10.10 dev eth0; has 'invalid gateway'; end
begin "route add: family mismatch";          ipc route add 10.1.1.0/24 via fe80::1 dev eth0; has 'different families'; end
begin "route add: bad src";                  ipc route add 10.1.1.0/24 dev eth0 src nope; has 'invalid source'; end
begin "route add: src family mismatch";      ipc route add 10.1.1.0/24 dev eth0 src ::1; has 'invalid source'; end
begin "route add: bad metric";               ipc route add 10.1.1.0/24 dev eth0 metric abc; has 'invalid metric'; end
begin "route add: negative metric";          ipc route add 10.1.1.0/24 dev eth0 metric -1; has 'invalid metric'; end
begin "route add: huge metric";              ipc route add 10.1.1.0/24 dev eth0 metric 99999999999; has 'invalid metric'; end
begin "route add: onlink needs via";         ipc route add 10.1.1.0/24 dev eth0 onlink; has '<onlink> needs <via>'; end
begin "route add: flag gets no value";       ipc route add 10.1.1.0/24 via 10.10.10.254 onlink dev eth0; has 'onlink'; has 'dev eth0'; end
begin "link set up";                         ipc link set eth0 up; has '\[ip link\] set device eth0: state up'; end
begin "link set down mtu name";              ipc link set eth0 down mtu 1400 name lan0; has 'state down mtu 1400 new name lan0'; end
begin "link set: up and down conflict";      ipc link set eth0 up down; has 'mutually exclusive'; end
begin "link set: nothing to set";            ipc link set eth0; has 'nothing to set'; end
begin "link set: mtu too small";             ipc link set eth0 mtu 10; has 'invalid mtu'; end
begin "link set: mtu too big";               ipc link set eth0 mtu 70000; has 'invalid mtu'; end
begin "link set: mtu not a number";          ipc link set eth0 mtu big; has 'invalid mtu'; end
begin "link set: bad new name";              ipc link set eth0 name a/b; has 'invalid new name'; end
begin "link set: missing device -> usage";   ipc link set; has 'usage: set <if>'; end
begin "link show";                           ipc link show; has 'show all devices'; end
begin "link show dev";                       ipc link show dev eth0; has 'show device eth0'; end
begin "neigh add";                           ipc neigh add 10.10.10.5 lladdr aa:bb:cc:dd:ee:ff dev eth0; has 'add neighbour 10.10.10.5 \(ipv4\) lladdr aa:bb:cc:dd:ee:ff on device eth0'; end
begin "neigh add: lladdr required";          ipc neigh add 10.10.10.5 dev eth0; has 'missing required keyword <lladdr>'; end
begin "neigh add: bad mac";                  ipc neigh add 10.10.10.5 lladdr aa:bb:cc dev eth0; has 'invalid lladdr'; end
begin "neigh add: non-hex mac";              ipc neigh add 10.10.10.5 lladdr zz:bb:cc:dd:ee:ff dev eth0; has 'invalid lladdr'; end
begin "neigh del: lladdr optional";          ipc neigh del 10.10.10.5 dev eth0; has 'delete neighbour 10.10.10.5 \(ipv4\) on device eth0'; end
begin "neigh show";                          ipc neigh show; has 'show all neighbours'; end
begin "ip alone lists the tree";             ipc; has 'available commands'; has 'Routing table'; lacks '\[ip '; end
begin "ip addr alone lists subcommands";     ipc addr; has 'available commands'; has 'flush'; end
begin "ip unknown object";                   ipc nothing; has 'unknown command <nothing>'; end
begin "ip: very long keyword value";         cli $XFS ip addr add 10.0.0.1/24 dev "$(pad 200)"; no_crash; has 'invalid device name'; end
begin "ip: ASan clean on full route command"; cli $XFS ip route add 10.20.0.0/16 via 10.10.10.254 dev eth0 src 10.10.10.10 metric 100 onlink; no_crash; has 'onlink'; end

echo "== REPL: ip"
begin "ip command in repl";                  repl $REPLS 'ip addr add 10.10.10.10/24 dev eth0' 'ip route add default via 10.10.10.254 dev eth0'; has '\[ip addr\] add address'; has '\[ip route\] add default route'; end
begin "ip error in repl keeps session";      repl $REPLS 'ip addr add bad dev eth0' 'ip link show'; has 'invalid address'; has 'command failed'; has 'show all devices'; end
begin "completion: ip subobjects";           comp $REPLS 'ip \t\t'; has 'addr +link +neigh +route'; end
begin "completion: ip route actions";        comp $REPLS 'ip route \t\t'; has 'add +del +get +replace +show'; end
begin "completion: ip addr a<Tab>";          comp $REPLS 'ip addr a\t'; has '> ip addr add $'; end
begin "completion: leaf help";               comp $REPLS 'ip addr add \t'; has '-> add <addr>'; end
begin "completion: no crash after keyword";  comp $REPLS 'ip addr add 10.0.0.1/24 dev \t\t'; no_crash; end

echo "== REPL: input loop"
begin "empty line is ignored";            repl $REPLS '' 'power on'; no_crash; lacks 'command failed'; has '\[fn\] power on'; end
begin "whitespace-only line is ignored";  repl $REPLS '   ' '\t' 'power on'; no_crash; lacks 'command failed'; has '\[fn\] power on'; end
begin "exit terminates, rc=0";            repl $REPLS 'exit'; rc_is 0; end
begin "lines after exit are not executed"; repl $REPLS 'exit' 'power on'; lacks '\[fn\] power on'; end
begin "EOF (Ctrl-D) terminates, rc=0";    repl $REPLS 'power on'; rc_is 0; has '\[fn\] power on'; end
begin "error does not end the session";   repl $REPLS 'xyz' 'power on'; has 'command failed'; has '\[fn\] power on'; end
begin "usage error does not end the session"; repl $REPLS 'chassis identify' 'power on'; has 'usage'; has '\[fn\] power on'; end
begin "state does not leak between lines (chassis scan twice)"; repl $REPLS 'chassis status' 'chassis status'; count 'chassis scan' 2; count 'chassis status: ON' 2; end
begin "chain state resets on every line"; repl $REPLS 'info show x' 'info show x'; count '\[fn\] info show' 2; end
begin "line of 300 chars does not crash"; repl $REPLS "power on $(pad 300)"; no_crash; end
begin "line of 4000 chars does not crash"; repl $REPLS "$(pad 4000)" 'power on'; no_crash; has '\[fn\] power on'; end
begin "'exit' as prefix of another word is not exit"; repl $REPLS 'exitx' 'power on'; has 'command failed'; has '\[fn\] power on'; end
begin "tabs between tokens";              repl $REPLS 'power\ton'; no_crash; has '\[fn\] power on'; end
begin "quoted argument keeps spaces" $X;  repl $REPLS 'info set "a b" c'; has 'info set a b=c'; end

echo "== REPL: completion (Tab)"
begin "Tab Tab on empty line lists root commands"; comp $REPLS '\t\t'; has 'chassis +info +ip +power'; end
begin "unique prefix completes: ch -> chassis";    comp $REPLS 'ch\t'; has '> chassis $'; end
begin "unique prefix completes: p -> power";       comp $REPLS 'p\t'; has '> power $'; end
begin "unique prefix completes: in -> info";       comp $REPLS 'in\t'; has '> info $'; end
begin "ambiguous root prefix lists info and ip";   comp $REPLS 'i\t\t'; has 'info +ip'; end
begin "subcommands listed after 'power '";         comp $REPLS 'power \t\t'; has 'off +on +status'; end
begin "ambiguous prefix lists candidates";         comp $REPLS 'power o\t\t'; has 'off +on'; lacks 'status'; end
begin "unique subcommand completes: power s";      comp $REPLS 'power s\t'; has '> power status $'; end
begin "extra spaces between tokens";               comp $REPLS 'power   o\t\t'; has 'off +on'; end
begin "tab between tokens";                        comp $REPLS 'power\to\t\t'; has 'off +on'; end
begin "leaf shows help instead of candidates";     comp $REPLS 'power on \t'; has '-> Power on the chassis'; end
begin "leaf with argument shows help";             comp $REPLS 'chassis identify \t'; has '-> Blink identify LED'; end
begin "parent with fn+next lists subcommands";     comp $REPLS 'chassis \t\t'; has 'bootdev +identify +policy +status'; end
begin "self-loop table lists commands";            comp $REPLS 'info \t\t\t'; has 'set +show +splash'; end
begin "unknown word: no candidates, no help";      comp $REPLS 'xyz \t\t'; no_crash; lacks '->'; end
begin "unknown word in the middle: no candidates"; comp $REPLS 'power xyz \t\t'; no_crash; lacks '->'; lacks 'off +on'; end
begin "ambiguous word before cursor: no candidates"; comp $REPLS 'power o \t\t'; no_crash; lacks '->'; end
begin "misconfigured entry does not crash completion"; comp $REPLS 'chassis asd \t\t'; no_crash; end
begin "completing a command whose help is missing"; comp $REPLS 'chassis asd\t'; no_crash; end
begin "very long line before Tab does not crash";  comp $REPLS "$(pad 400) \t\t"; no_crash; end
begin "very long line before Tab, 255 boundary";   comp $REPLS "$(pad 255)\t"; no_crash; end
begin "completion after args in a self-loop" $X;   comp $REPLS 'info set a b \t\t\t'; has 'set +show +splash'; end
begin "completion works after a failed command";   OUT=$(printf 'xyz\np\t\x15\n' | $REPLS 2>&1); RC=$?; OUT=$(norm <<<"$OUT"); has 'command failed'; has '> power $'; end

echo "== REPL: keyword completion"
begin "keywords after a positional argument";      comp $REPLS 'ip a a 0.0.0.0/24 \t\t'; has '^broadcast +dev +label *$'; end
begin "reported case: ip a a 0.0.0.0/24 d 1 l<Tab>"; comp $REPLS 'ip a a 0.0.0.0/24 d 1 l\t'; has '> ip a a 0.0.0.0/24 d 1 label $'; end
begin "keyword prefix completes: d -> dev";        comp $REPLS 'ip addr add 10.0.0.1/24 d\t'; has 'dev $'; end
begin "used keywords are not offered again";       comp $REPLS 'ip addr add 10.0.0.1/24 dev eth0 \t\t'; has '^broadcast +label *$'; end
begin "all keywords used: nothing to offer";       comp $REPLS 'ip addr add 10.0.0.1/24 dev e label l broadcast b \t\t'; no_crash; lacks 'broadcast +dev'; end
begin "value position: help, no keyword list";     comp $REPLS 'ip addr add 10.0.0.1/24 dev \t'; has '-> add <addr>'; lacks '^broadcast +'; end
begin "keyword abbreviation inside the walk";      comp $REPLS 'ip route add 10.1.1.0/24 v 10.0.0.1 \t\t'; has '^dev +metric +onlink +src *$'; end
begin "flag keyword needs no value";               comp $REPLS 'ip route add 10.1.1.0/24 via 10.0.0.1 onlink \t\t'; has '^dev +metric +src *$'; end
begin "link set: up/down/mtu/name after device";   comp $REPLS 'ip link set eth0 \t\t'; has 'down +mtu +name +up'; end
begin "unknown keyword in the line: no completion"; comp $REPLS 'ip addr add 10.0.0.1/24 foo \t\t'; no_crash; lacks 'broadcast'; end
begin "inside the positional argument: nothing";   comp $REPLS 'ip addr add 10.\t\t'; no_crash; lacks 'dev'; end
begin "neigh add keywords";                        comp $REPLS 'ip neigh add 10.0.0.5 \t\t'; has 'dev +lladdr'; end
begin "command without keywords: tokens after args"; comp $REPLS 'chassis identify on \t'; no_crash; lacks 'broadcast'; end
begin "self-loop: after positional args the command is a leaf (help)"; comp $REPLS 'info set a b \t'; has '-> set <what> <to>'; end

echo "== CLI/REPL parity (same input -> same dispatcher output)"
parity() { # parity "tokens..."
	local line=$1 c r
	cli $XF $line;       c=$(grep -v -e '^┌─<' -e '^$' <<<"$OUT")
	repl $REPL "$line";  r=$(grep -v -e '^> ' -e '^$' -e '^error: command failed' <<<"$OUT")
	[ "$c" = "$r" ] || bad "differs for '$line'"
}
for l in "power on" "power off" "power status" "power" "pow on" "p on" "power o" "power xyz" "xyz" \
         "chassis" "chassis status" "chassis identify" "chassis identify on" "ch bootdev pxe" \
         "chassis asd x" "info set a b" "info set a" "info splash" "info show show" "in s" \
         "ip addr add 10.10.10.10/24 dev eth0" "ip route add default via 10.10.10.254 dev eth0" \
         "ip route add 10.10.10.1 dev eth0" "ip link set eth0 up" "ip addr add 1.1.1.1/99 dev eth0"; do
	begin "parity: $l"; parity "$l"; end
done

echo "== memory: completion allocations (LeakSanitizer)"
if [ $HAVE_SAN = 1 ]; then
	begin "args_complete: no leaks, no ASan/UBSan errors"; OUT=$("$B/comp_leak_asan" 2>&1); RC=$?; OUT=$(norm <<<"$OUT"); rc_is 0; has '^ok$'; lacks 'Sanitizer|runtime error'; end
fi

echo
printf 'passed %d, failed %d, expected-fail %d, unexpectedly-passed %d\n' $pass $fail $xfail $xpass
[ $HAVE_SAN = 1 ] || echo "note: ASan/UBSan build unavailable, limit tests ran without sanitizers"
[ $fail -eq 0 ]

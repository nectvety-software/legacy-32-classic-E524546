from __future__ import annotations

from dataclasses import dataclass
import hashlib
import os
from pathlib import Path
import re

VQEAF_OS_REPOSITORY = 'https://github.com/qeafivels/VQEAF-OS'
PROFILE_TYPES = {
    'production': ('web', 'text'),
    'lua-beta': ('web', 'text', 'lua'),
    'snake-demo': ('web', 'text'),
}
TRUST_HEADERS = {
    'production': 'QeappTrustKey.h',
    'lua-beta': 'QeappTrustKeyLuaBeta.h',
    'snake-demo': 'SnakeDemoTrustKey.h',
}
PROFILE_ENVS = {
    'production': 'vqeaf_os',
    'lua-beta': 'vqeaf_lua_beta',
    'snake-demo': 'vqeaf_snake_demo',
}
TRUSTED_QEAPP_CONTRACT = {
    'production': {
        'QeappFormat.h': frozenset({'79d0514cf2eb42a4ab19ceccb696f42d182d5c230dce73c11adb58f35ad81532'}),
        'QeappFormat.cpp': frozenset({
            '1587dc851cbae9875d6627f039a4b4de6abe4bbcb2e4a67e5ff4338dfb6c9930',
            '5b4b1901dffcb5c4dc78f189c8c3e8116cda4dbee3ec29e0365c4ae6d4904ba0',
        }),
        'QeappSignature.h': frozenset({'f4d7bccb0fbac3b72e13ef946793f17bb1bb452fd389e3080c55c0d462f2d2be'}),
        'QeappSignature.cpp': frozenset({
            '7a26ae0a7207e07437232665973634e1f6855dd15f1063d411ea722e1a92b83d',
            'bf3e22d98aa182774221b3007039450381c72c5d221aff6cf34f9920a9693263',
        }),
        'QeappVersion.h': frozenset({'e3beba7e4f2b27354784c58753b4f4371c7efe9f50f97d570cd43949bd20e78f'}),
        'QeappVersion.cpp': frozenset({'88c01ed68018f965a4fa3c164e3d471d3c87546312d6f29b66a5860eeb832dfb'}),
        'AppInstallerService.h': frozenset({
            '48e4c9c39260dc7fa7c3c8557d346a0c6f46ef14534a7e410f14d0a3698aef94',
            'f5be5619101a262b469a588cadd27c0107d6f904149f8fb47e172c0009035f97',
        }),
        'AppInstallerService.cpp': frozenset({
            '27e551227585deba8b979d259175888436f964287f59eeefbbdcb3caad6bd4bf',
            '807fef52ab80390001005db2d0bd653a399e7f2b0a4509b09994319bf4a5152a',
        }),
    },
    'lua-beta': {
        'QeappFormat.h': frozenset({'79d0514cf2eb42a4ab19ceccb696f42d182d5c230dce73c11adb58f35ad81532'}),
        'QeappFormat.cpp': frozenset({'1587dc851cbae9875d6627f039a4b4de6abe4bbcb2e4a67e5ff4338dfb6c9930'}),
        'QeappSignature.h': frozenset({'f4d7bccb0fbac3b72e13ef946793f17bb1bb452fd389e3080c55c0d462f2d2be'}),
        'QeappSignature.cpp': frozenset({'7a26ae0a7207e07437232665973634e1f6855dd15f1063d411ea722e1a92b83d'}),
        'QeappVersion.h': frozenset({'e3beba7e4f2b27354784c58753b4f4371c7efe9f50f97d570cd43949bd20e78f'}),
        'QeappVersion.cpp': frozenset({'88c01ed68018f965a4fa3c164e3d471d3c87546312d6f29b66a5860eeb832dfb'}),
        'AppInstallerService.h': frozenset({'48e4c9c39260dc7fa7c3c8557d346a0c6f46ef14534a7e410f14d0a3698aef94'}),
        'AppInstallerService.cpp': frozenset({'27e551227585deba8b979d259175888436f964287f59eeefbbdcb3caad6bd4bf'}),
    },
    'snake-demo': {
        'QeappFormat.h': frozenset({'79d0514cf2eb42a4ab19ceccb696f42d182d5c230dce73c11adb58f35ad81532'}),
        'QeappFormat.cpp': frozenset({
            '1587dc851cbae9875d6627f039a4b4de6abe4bbcb2e4a67e5ff4338dfb6c9930',
            '5b4b1901dffcb5c4dc78f189c8c3e8116cda4dbee3ec29e0365c4ae6d4904ba0',
        }),
        'QeappSignature.h': frozenset({'f4d7bccb0fbac3b72e13ef946793f17bb1bb452fd389e3080c55c0d462f2d2be'}),
        'QeappSignature.cpp': frozenset({
            '7a26ae0a7207e07437232665973634e1f6855dd15f1063d411ea722e1a92b83d',
            'bf3e22d98aa182774221b3007039450381c72c5d221aff6cf34f9920a9693263',
        }),
        'QeappVersion.h': frozenset({'e3beba7e4f2b27354784c58753b4f4371c7efe9f50f97d570cd43949bd20e78f'}),
        'QeappVersion.cpp': frozenset({'88c01ed68018f965a4fa3c164e3d471d3c87546312d6f29b66a5860eeb832dfb'}),
        'AppInstallerService.h': frozenset({
            '48e4c9c39260dc7fa7c3c8557d346a0c6f46ef14534a7e410f14d0a3698aef94',
            'f5be5619101a262b469a588cadd27c0107d6f904149f8fb47e172c0009035f97',
        }),
        'AppInstallerService.cpp': frozenset({
            '27e551227585deba8b979d259175888436f964287f59eeefbbdcb3caad6bd4bf',
            '807fef52ab80390001005db2d0bd653a399e7f2b0a4509b09994319bf4a5152a',
        }),
    },
}
TRUSTED_LUA_MAIN_SHA256 = 'df80eff4ba0186ab3c78d8accc1dbd3b0a79b30155a2f6d4d808cbf45fdc3b35'
TRUSTED_SIGNER_SHA256 = {
    'production': frozenset({
        '8726ce6ff5aef9a7a8b9e8ebd550c5764153ab319bd2ad465c55634b040218f0',
        '322afa26c0505c20699106b93597c08aed4c031fa7290cc707e8ddfe8f87f36e',
    }),
    'lua-beta': frozenset({
        '8726ce6ff5aef9a7a8b9e8ebd550c5764153ab319bd2ad465c55634b040218f0',
    }),
    'snake-demo': frozenset({
        '8726ce6ff5aef9a7a8b9e8ebd550c5764153ab319bd2ad465c55634b040218f0',
        '322afa26c0505c20699106b93597c08aed4c031fa7290cc707e8ddfe8f87f36e',
    }),
}
LUA_ARCHIVE_SHA256 = '4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae'
LUA_LICENSE_SHA256 = 'd83103413067359fa162d5c465c076d6a9a25f8d678f06b7ed2a7c9d9e4fa704'
LUA_LIBRARY_JSON = '{"name":"VqeafLua54","version":"5.4.8","build":{"includeDir":"src"},"frameworks":"*","platforms":"*"}\n'
LUA_UPSTREAM_TEXT = (
    'Lua 5.4.8 original source: https://www.lua.org/ftp/lua-5.4.8.tar.gz\n'
    f'SHA256: {LUA_ARCHIVE_SHA256}\n'
    'License: MIT (see LICENSE.lua)\n'
)
LUA_SOURCE_SHA256 = {
    'lapi.h': '9522209670086adfbb11379416078a7f4dafe154043e320cd435fa1811aaabfb',
    'lauxlib.h': '2d9e18e577a6062646268e7f7228264c28bd9b972c186e1af81b8357179c0ab4',
    'lcode.h': 'c5ad73fb12fd3e9f6b6a5f37fea5b60e4c09f8a262e9ac3ca85e8e047f745cea',
    'lctype.h': 'c800311872ecba642f6d013b290eda83ba6cd8df8061e25cdca1dd4adcbed8b5',
    'ldebug.h': 'b0fd38f6461251ec481cf5633d622569cef939067e37cac23331d9f96b601702',
    'ldo.h': 'f150c09811f1d1f302dd9eafea36d4003a834c7af24fb6329b4e9a1970c3dfec',
    'lfunc.h': '81dc02d55ae8000461ab90bb9689d2600ba51f2ea9ac9fb019386995c07e3e4a',
    'lgc.h': 'c83d3b8e31345c0aacd37442954a7f2434312ee48f98a8b9ae4fc8aad26d0c99',
    'ljumptab.h': '6c3e9858b9dad03ccd61445ab0bff0eb6ddfa5e731e044f93350fe6c6b2ad8d5',
    'llex.h': '5ce0302528260ed6a40ec3ebf5e27d3bc108242d9dca57e8cdbe80fe34e13410',
    'llimits.h': 'd588bdac3c8e1adc88df3dc92c950a1bfa29663d15f9221f58e2aa553b05a422',
    'lmem.h': '8318a170a1d4bf21c3cb4deb1f8661f1dc7cf4eb688e4ec9a1e7ea16bfc3b5cf',
    'lobject.h': '8539beae7dd9a08efc09b04d64c09663e9afc0f2ad342abc612bb000c0c9be70',
    'lopcodes.h': 'b3f511933be3039a4b92ba79601b2e5daf75b34f268bfc757adf3e24ea4f1ec9',
    'lopnames.h': 'fbdbebc96b136efc6165cba1ac2b311b0c52413aaef59ae9ec750748f13c9e9a',
    'lparser.h': '68c34ef2d6e5a227f7cff18436a42c6739b1470ce12f059f168d9bb69fc8d49c',
    'lprefix.h': '626785d4eda75e9435f0ac21a780ae3b11f8b27dc36934e637a5c2305f906531',
    'lstate.h': 'ac1f8464e0d0dc6c9d22220bc3b5ec682c91de81a661bfb5f971e22b8fce0a9a',
    'lstring.h': 'a8cb0d737276f6a41071a0776ee1d8aa13cf02fa7c79ea096d202b14ea792e31',
    'ltable.h': '1c8736b87aa408ef2afebfe693f46ae5424d658d798fb2a8a88b6868cf5569b5',
    'ltm.h': '5b2f6e59951edeef6e71bebe5ac4351f4b3602ee0461729308b80dcee8cea910',
    'lua.h': '3b77329f9deed929a5cbe7a7d5fc81ebbb63fd869a49e0a9302fe9240caf1930',
    'luaconf.h': 'af243c94bee6d2601383e8b65ed66a0d1b4ab3b7418ff903fcd0679912160e3a',
    'lualib.h': '4ea0f67d3e467f5d6a327616e22a3c2da8e03d47a3c3660916275f2842f521ea',
    'lundump.h': '1137c0c9d8654ce223374d21c52a599fd1cc1ccb869801f87a27238a7bf55e8e',
    'lvm.h': 'aba601b887236d5e135392e0c49370df3b5a9ac5c66ab2d44156b3244f2dcef3',
    'lzio.h': '9673f62eac2d0fe3010f9d23148be7b8113319fdf4c4490015cd9feae2ecaad4',
    'lapi.c': '6afef609bc1d93280a1da898c82734b14ca94d98f72d786c397c5988e6cc6e98',
    'lauxlib.c': '50ef12f21926fa3b80d1e9e611c5e4669012efe792b5a3515fd5a7ee1cc796e3',
    'lbaselib.c': '5dc9a64468c1ac31e59066c7d3597eeb8ea4ac8fc19a257a74c431624300b487',
    'lcode.c': '2cf13c7f8205e9f07c2ccf1fedc3f0f0a05d238c194b176c72cc22b2cc96210e',
    'lctype.c': '3e21ae6a8faab3ed470ae0de19360da6b4e21a0a0f8572f502f7e13d590186f8',
    'ldebug.c': '06d065d1f26c29546cc74b61a50267cb70567ae538691f190bf15ca5cd29d308',
    'ldo.c': 'ff588c58143d3dd75157004a5a785044beb9a6746378ccc6e46d4b3a303657c2',
    'ldump.c': '3c05402c5e282e06c2218f2469b1f38e1b84c783b9387aec7740c60b287d82c9',
    'lfunc.c': '38809428b76fafa79db9f65b7248cc0c02c9b696e7410fc4ab1699940768f1b2',
    'lgc.c': 'bf28b521954d66ba02a49569c0f7bf37ebdd4f0a62fde4312284e817fb1e42fe',
    'llex.c': '5cfbaabf3dc0d75835aaafc37fcd87a92f3dba451e002732747b602fda122958',
    'lmathlib.c': 'da5c7d74cffce22f0a9eb468e6e7e80b543ea6bec5c21aefb8cf285a3556722a',
    'lmem.c': '6154c2865bec97e61a92d8284be7e8ba21818f6edc95f5d72adb5dcf3e9532ca',
    'lobject.c': '7a9748e38c52e45bc468cae74ccea08b98f8c53a523c1b0f9a117e5cf162c921',
    'lopcodes.c': 'bdc9d39fd8bf1a350c0249f726c5f61bef64b122b93a0fbc172b0328dc01f594',
    'lparser.c': 'c5507307586c1a2336a7722229f7b350ae9c548b854933b370ad150e9e6e7f84',
    'lstate.c': '5b2003a6bd7c8d98af10f4851ced899581130d5ba9db831cedcc7ac53314e806',
    'lstring.c': 'b6162f3a65735a902911ffafcafe1db0994c3b0df441581dcbb04dca6bc0b71d',
    'lstrlib.c': '3257cd857653560773947d70b65c8b8bf8ef43f920ed449587fa662224d859e1',
    'ltable.c': '27a711276b1e22aaea3b9182e2d17693f598cfe9bc7fd203c05d400df6673853',
    'ltablib.c': '3d430a6f9cde66d0b791444c6164e7559ccf62b7cdc3a6325882408c6806e26c',
    'ltm.c': '7cbe645ed7e1161d31eb32ea467d8220b592c0acfa6107f0194487f0ddbf954b',
    'lundump.c': '7f7b078026e162161d402feba73a69cbe66a98558ec0745a445a595f42eac688',
    'lutf8lib.c': 'ed24c3faa8abda78ee6df53f011475ab9565a2e3ace64e714bd412c90160742c',
    'lvm.c': '88b10a2f1f539cdfbefac818c64ceee59ac1b5f55038643637109a98834bb926',
    'lzio.c': 'fba61ceeeb257a3fe5f63310f21e14fc26c4be647583dcf8e74cbc7fb7e43e8a',
}
LUA_CORE = frozenset(LUA_SOURCE_SHA256)


class CompatibilityError(ValueError):
    pass


def _is_reparse_path(path: Path) -> bool:
    try:
        attributes = getattr(path.lstat(), 'st_file_attributes', 0)
    except OSError:
        return False
    return path.is_symlink() or bool(attributes & 0x400)


@dataclass(frozen=True)
class TrustAnchor:
    key_id: int
    public_key: bytes
    header: Path

    @property
    def public_key_sha256(self) -> str:
        return hashlib.sha256(self.public_key).hexdigest()


def resolve_profile(app_type: str, requested: str = 'auto') -> str:
    if requested == 'auto':
        requested = 'lua-beta' if app_type == 'lua' else 'production'
    if requested not in PROFILE_TYPES:
        raise CompatibilityError(f'Unsupported firmware profile: {requested}')
    if app_type == 'lua' and requested != 'lua-beta':
        raise CompatibilityError('Lua apps require the explicit vqeaf_lua_beta firmware profile')
    if app_type not in PROFILE_TYPES[requested]:
        supported = ', '.join(PROFILE_TYPES[requested])
        raise CompatibilityError(f'{app_type} apps require a firmware profile supporting: {supported}')
    return requested


def _parse_integer(value: str) -> int:
    value = value.strip()
    return int(value, 16) if value.lower().startswith('0x') else int(value, 10)


def _strip_c_comments(text: str) -> str:
    text = re.sub(r'#if\s+0\b.*?#endif', '', text, flags=re.S)
    return re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.S)


def parse_trust_anchor(path: Path, profile: str = 'production') -> TrustAnchor:
    source = path.read_text(encoding='utf-8')
    if re.search(r'-----BEGIN [A-Z ]*PRIVATE KEY-----', source, re.I):
        raise CompatibilityError('Firmware trust header contains private key material')
    if re.search(r'^\s*#\s*error\b', source, re.M):
        raise CompatibilityError('Firmware trust header contains an active #error directive')
    text = _strip_c_comments(source)
    directives = re.findall(r'^\s*#\s*(if[^\\\n]*|ifdef[^\\\n]*|ifndef[^\\\n]*|else|endif)\b', text, re.M)
    if directives:
        if path.name != 'QeappTrustKey.h' or profile != 'production':
            raise CompatibilityError('Firmware trust header uses an unsupported conditional profile')
        branch = re.search(
            r'#if\s+defined\(VQEAF_LUA_BETA_TRUST\)\s*&&\s*VQEAF_LUA_BETA_TRUST\s*'
            r'#include\s+"QeappTrustKeyLuaBeta\.h"\s*#else\s*(?P<production>.*?)\s*#endif\b',
            text, re.S,
        )
        if branch is None:
            raise CompatibilityError('Firmware production trust branch is not the supported QEAPP/2 structure')
        text = branch.group('production')
    key_matches = re.findall(r'QEAPP_TRUST_KEY_ID\s*=\s*(0x[0-9a-fA-F]+|\d+)', text)
    point_matches = re.findall(r'QEAPP_TRUST_PUBKEY\s*\[\s*65\s*\]\s*=\s*\{(.*?)\};', text, re.S)
    if len(key_matches) != 1 or len(point_matches) != 1:
        label = 'Lua beta publisher is not provisioned' if 'LuaBeta' in path.name else 'firmware trust anchor is not uniquely provisioned'
        raise CompatibilityError(label)
    try:
        public_key = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]{1,2})', point_matches[0]))
    except ValueError as err:
        raise CompatibilityError('Firmware trust anchor contains an invalid public-key byte') from err
    if len(public_key) != 65 or public_key[0] != 4:
        raise CompatibilityError('Firmware trust anchor is not an uncompressed P-256 public point')
    try:
        key_id = _parse_integer(key_matches[0])
    except ValueError as err:
        raise CompatibilityError('Firmware trust key ID is not a valid integer') from err
    if not 0 <= key_id <= 0xFFFFFFFF:
        raise CompatibilityError('Firmware trust key ID is out of range')
    try:
        from cryptography.hazmat.primitives.asymmetric import ec
        ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), public_key)
    except ImportError as err:
        raise CompatibilityError('cryptography is required to validate the P-256 firmware trust anchor') from err
    except ValueError as err:
        raise CompatibilityError('Firmware trust anchor is not a valid P-256 curve point') from err
    return TrustAnchor(key_id, public_key, path)


def trust_anchor(root: Path, profile: str) -> TrustAnchor:
    if profile not in TRUST_HEADERS:
        raise CompatibilityError(f'Unsupported firmware profile: {profile}')
    path = root / 'src' / 'services' / TRUST_HEADERS[profile]
    if not path.is_file():
        if profile == 'lua-beta':
            raise CompatibilityError('Lua beta public key is missing; provision the beta publisher before building')
        raise CompatibilityError(f'Firmware trust header is missing: {TRUST_HEADERS[profile]}')
    return parse_trust_anchor(path, profile)


def _firmware_version(root: Path) -> str:
    version_file = root / 'src' / 'core' / 'BuildVersion.h'
    if not version_file.is_file():
        return 'unknown'
    text = version_file.read_text(encoding='utf-8')
    for macro in ('VQEAF_OS_VERSION_TEXT', 'VQEAF_OS_VERSION'):
        match = re.search(rf'{macro}\s+"([^"]+)"', text)
        if match:
            return match.group(1)
    return 'unknown'


def _platformio_sections(text: str) -> dict[str, dict[str, list[str]]]:
    sections: dict[str, dict[str, list[str]]] = {}
    current: str | None = None
    last_key: str | None = None
    for raw in text.splitlines():
        line = raw.strip()
        section = re.fullmatch(r'\[([^]]+)]', line)
        if section:
            current = section.group(1)
            if current.lower() == 'platformio':
                current = 'platformio'
            if current.startswith('env:'):
                current = current[4:]
            sections.setdefault(current, {})
            last_key = None
            continue
        if not current or not line or line.startswith((';', '#')):
            continue
        if raw[:1].isspace() and last_key in sections[current]:
            sections[current][last_key].append(line)
        elif '=' in line:
            key, value = line.split('=', 1)
            last_key = key.strip().lower()
            sections[current].setdefault(last_key, []).append(value.strip())
    return sections


def _resolve_option(sections: dict[str, dict[str, list[str]]], env: str, option: str,
                    active: set[str] | None = None) -> list[str]:
    active = set() if active is None else active
    if env in active or env not in sections:
        raise CompatibilityError(f'PlatformIO environment inheritance is invalid at {env}')
    active.add(env)
    section = sections[env]
    if option in section:
        values: list[str] = []
        for value in section[option]:
            references = re.findall(r'\$\{env:([^}.]+)\.([^}]+)}', value)
            if references and not all(reference_option == option for _, reference_option in references):
                raise CompatibilityError(f'Unsupported PlatformIO interpolation in {env}.{option}')
            remaining = re.sub(r'\$\{env:[^}.]+\.[^}]+}', '', value)
            if '${' in remaining:
                raise CompatibilityError(f'Non-PlatformIO interpolation in {env}.{option}')
            for parent, _ in references:
                values.extend(_resolve_option(sections, parent.removeprefix('env:'), option, active))
            values.append(value)
        return values
    for parent in section.get('extends', []):
        return _resolve_option(sections, parent.removeprefix('env:'), option, active)
    return []


def _chain_has_option(sections: dict[str, dict[str, list[str]]], env: str, option: str,
                      active: set[str] | None = None) -> bool:
    active = set() if active is None else active
    if env in active or env not in sections:
        raise CompatibilityError(f'PlatformIO environment inheritance is invalid at {env}')
    active.add(env)
    section = sections[env]
    if option in section:
        return True
    return any(
        _chain_has_option(sections, parent.removeprefix('env:'), option, active)
        for parent in section.get('extends', [])
    )


def _effective_build_flags(sections: dict[str, dict[str, list[str]]], env: str) -> list[str]:
    return (
        _resolve_option(sections, env, 'build_flags')
        + _resolve_option(sections, env, 'build_src_flags')
    )


def _strip_flag_comment(flag: str) -> str:
    quote = None
    escaped = False
    for index, char in enumerate(flag):
        if quote is not None:
            if escaped:
                escaped = False
            elif char == '\\':
                escaped = True
            elif char == quote:
                quote = None
        elif char in ('"', "'"):
            quote = char
        elif char in (';', '#'):
            return flag[:index]
    return flag


def _profile_flags_ready(flags: list[str], profile: str) -> bool:
    cleaned = [_strip_flag_comment(flag).strip() for flag in flags]
    text = '\n'.join(cleaned)
    assignments = {}
    duplicates = set()
    for match in re.finditer(r'(?:^|\s)-D\s*([A-Za-z_][A-Za-z0-9_]*)(?:=([^\s]+))?', text):
        name, value = match.group(1), match.group(2) or '1'
        if name in assignments:
            duplicates.add(name)
        assignments[name] = value
    if duplicates or re.search(r'(?:^|\s)-U(?:\s|=)', text):
        return False
    beta = assignments.get('VQEAF_LUA_BETA_TRUST')
    snake = assignments.get('VQEAF_SNAKE_DEMO_KEY')
    lua = assignments.get('VQEAF_ENABLE_LUA')
    psram = assignments.get('QE_LUA_PSRAM_ALLOC')
    if 'QEAPP_TRUST_KEY_HEADER' in assignments:
        return False
    if profile == 'production':
        return not any(value is not None for value in (beta, snake, lua, psram))
    if profile == 'lua-beta':
        return beta == '1' and snake is None and lua == '1' and psram == '1'
    return snake == '1' and beta is None and lua is None and psram is None


def evaluate_platformio_profile(sections: dict[str, dict[str, list[str]]], env: str,
                                profile: str) -> list[str]:
    unsupported_options = ('extra_scripts', 'extra_script', 'src_build_flags', 'debug_build_flags')
    if 'extra_configs' in sections.get('platformio', {}):
        raise CompatibilityError('PlatformIO extra_configs can change the selected trust/runtime profile')
    if any(_chain_has_option(sections, env, option) for option in unsupported_options):
        raise CompatibilityError('Dynamic or legacy PlatformIO options can change the selected trust/runtime profile')
    override_names = (
        'PLATFORMIO_BUILD_FLAGS', 'PLATFORMIO_BUILD_SRC_FLAGS', 'PLATFORMIO_BUILD_UNFLAGS',
        'PLATFORMIO_EXTRA_SCRIPTS', 'PLATFORMIO_EXTRA_CONFIGS', 'PLATFORMIO_DEBUG_BUILD_FLAGS',
        'BUILD_FLAGS', 'SRC_BUILD_FLAGS', 'EXTRA_SCRIPT',
    )
    active_overrides = [name for name in override_names if os.environ.get(name)]
    if active_overrides:
        raise CompatibilityError('PlatformIO environment overrides can change the selected trust/runtime profile')
    flags = _effective_build_flags(sections, env)
    unflags = _resolve_option(sections, env, 'build_unflags')
    profile_names = ('VQEAF_ENABLE_LUA', 'VQEAF_LUA_BETA_TRUST', 'VQEAF_SNAKE_DEMO_KEY',
                     'QE_LUA_PSRAM_ALLOC', 'QEAPP_TRUST_KEY_HEADER')
    if any(name in '\n'.join(unflags) for name in profile_names):
        raise CompatibilityError('Dynamic scripts or unflags can change the selected trust/runtime profile')
    if not _profile_flags_ready(flags, profile):
        raise CompatibilityError(f'PlatformIO profile {env} does not match {profile}')
    return flags


def inspect_firmware(app_type: str, root: Path, requested_profile: str = 'auto') -> dict:
    root = Path(root).resolve(strict=True)
    profile = resolve_profile(app_type, requested_profile)
    checks: dict[str, dict] = {}

    def add(name: str, ready: bool, detail: str, remedy: str = '') -> None:
        value = {'status': 'READY' if ready else 'MISSING', 'detail': detail}
        if remedy and not ready:
            value['action'] = remedy
        checks[name] = value

    signer = root / 'tools' / 'build_qeapp.py'
    services = root / 'src' / 'services'
    platformio = root / 'platformio.ini'
    signer_text = signer.read_text(encoding='utf-8') if signer.is_file() else ''
    ini_text = platformio.read_text(encoding='utf-8') if platformio.is_file() else ''
    sections = _platformio_sections(ini_text)
    env_name = PROFILE_ENVS[profile]
    signer_hash = hashlib.sha256(signer_text.replace('\r\n', '\n').encode('utf-8')).hexdigest() if signer.is_file() else None
    signer_ready = (
        signer.is_file() and 'QEAPP2' in signer_text and 'QSIGP256' in signer_text
        and 'secp256r1' in signer_text and signer_hash in TRUSTED_SIGNER_SHA256[profile]
    )
    contract_hashes = {}
    contract_ready = True
    for name, allowed_hashes in TRUSTED_QEAPP_CONTRACT[profile].items():
        contract_file = services / name
        contract_hash = (
            hashlib.sha256(contract_file.read_text(encoding='utf-8').replace('\r\n', '\n').encode('utf-8')).hexdigest()
            if contract_file.is_file() else None
        )
        contract_hashes[name] = contract_hash
        contract_ready = contract_ready and contract_hash in allowed_hashes
    add('signer', signer_ready, str(signer), 'Select a VQEAF-OS source containing the official QEAPP/2 signer')
    add('parser_verifier', contract_ready, str(services), 'Select a matching VQEAF-OS parser and signature-verifier contract')
    try:
        effective_flags = evaluate_platformio_profile(sections, env_name, profile)
        flags_ready = True
        flags_detail = f'Effective PlatformIO flags for {env_name}'
    except CompatibilityError as exc:
        effective_flags = []
        flags_ready = False
        flags_detail = str(exc)
    add(
        'firmware_profile',
        flags_ready,
        flags_detail,
        f'Select a non-conflicting {env_name} PlatformIO profile',
    )
    if app_type == 'lua':
        add(
            'lua_builder',
            '--lua' in signer_text and '--enable-lua-experimental' in signer_text,
            'Firmware signer exposes the experimental Lua payload path',
            'Use the QEAPP Studio firmware overlay with Lua beta support',
        )
        runtime = root / 'src' / 'lua' / 'QeLuaRuntime.cpp'
        runtime_header = root / 'src' / 'lua' / 'QeLuaRuntime.h'
        studio_root = Path(__file__).resolve().parents[2]
        host_runtime = studio_root / 'runtime' / 'src' / 'QeLuaRuntime.cpp'
        host_header = studio_root / 'runtime' / 'include' / 'QeLuaRuntime.h'
        expected_runtime = (
            '#if defined(VQEAF_ENABLE_LUA) && VQEAF_ENABLE_LUA\n'
            + host_runtime.read_text(encoding='utf-8').replace('\r\n', '\n')
            + '\n#endif // VQEAF_ENABLE_LUA\n'
        )
        target_runtime = runtime.read_text(encoding='utf-8').replace('\r\n', '\n') if runtime.is_file() else ''
        target_header = runtime_header.read_bytes().replace(b'\r\n', b'\n') if runtime_header.is_file() else b''
        expected_header = host_header.read_bytes().replace(b'\r\n', b'\n')
        add(
            'lua_runtime',
            target_runtime == expected_runtime and target_header == expected_header,
            'Firmware Lua VM source and header exactly match the Studio runtime',
            'Use the matching QEAPP Studio firmware overlay with Lua beta support',
        )
        main_source = root / 'src' / 'main.cpp'
        main_hash = (
            hashlib.sha256(main_source.read_text(encoding='utf-8').replace('\r\n', '\n').encode('utf-8')).hexdigest()
            if main_source.is_file() else None
        )
        add(
            'lua_integration',
            main_hash == TRUSTED_LUA_MAIN_SHA256,
            'Firmware main.cpp matches the pinned Lua launch/frame/input integration',
            'Use the matching QEAPP Studio firmware overlay with Lua beta support',
        )
        library_root = root / 'lib' / 'VqeafLua54'
        vendor = library_root / 'src'
        root_links = [
            name for name, path in (('VqeafLua54', library_root), ('src', vendor))
            if _is_reparse_path(path)
        ]
        vendor_paths = list(vendor.rglob('*')) if vendor.is_dir() and not root_links else []
        actual_lua = {path.name for path in vendor_paths if path.is_file() and path.parent == vendor}
        actual_relative = {
            path.relative_to(vendor).as_posix() for path in vendor_paths
            if path.is_file() or path.is_symlink()
        }
        unexpected_lua = sorted(actual_relative - set(LUA_SOURCE_SHA256))
        linked_lua = root_links + sorted(
            path.relative_to(vendor).as_posix() for path in vendor_paths if path.is_symlink()
        )
        missing_lua = []
        invalid_lua = []
        for name, expected_hash in LUA_SOURCE_SHA256.items():
            source_file = vendor / name
            if name not in actual_lua or name in linked_lua:
                missing_lua.append(name)
            elif hashlib.sha256(source_file.read_bytes()).hexdigest() != expected_hash:
                invalid_lua.append(name)
        upstream = library_root / 'UPSTREAM.txt'
        upstream_bytes = upstream.read_bytes() if upstream.is_file() else b''
        library_file = library_root / 'library.json'
        library_bytes = library_file.read_bytes() if library_file.is_file() else b''
        license_file = library_root / 'LICENSE.lua'
        license_hash = hashlib.sha256(license_file.read_bytes()).hexdigest() if license_file.is_file() else None
        manifest_ready = (
            upstream_bytes == LUA_UPSTREAM_TEXT.encode('utf-8')
            and library_bytes == LUA_LIBRARY_JSON.encode('utf-8')
            and license_hash == LUA_LICENSE_SHA256
        )
        source_detail = (
            'Official Lua 5.4.8 source set and pinned archive metadata are installed'
            if not missing_lua and not invalid_lua and not unexpected_lua and not linked_lua and manifest_ready else
            ('Missing source: ' + ', '.join(missing_lua) if missing_lua else
             ('Hash mismatch: ' + ', '.join(invalid_lua) if invalid_lua else
              ('Unexpected source: ' + ', '.join(unexpected_lua) if unexpected_lua else
               ('Symlink source: ' + ', '.join(linked_lua) if linked_lua else
                'Pinned archive metadata/license is missing'))))
        )
        add(
            'lua_source',
            not missing_lua and not invalid_lua and not unexpected_lua and not linked_lua and manifest_ready,
            source_detail,
            'Run tools/bootstrap_lua.py against the selected firmware checkout',
        )

    if profile == 'lua-beta':
        dispatch_file = services / 'QeappTrustKey.h'
        dispatch_text = _strip_c_comments(
            dispatch_file.read_text(encoding='utf-8') if dispatch_file.is_file() else ''
        )
        dispatch_ready = re.search(
            r'#if\s+defined\(VQEAF_LUA_BETA_TRUST\)\s*&&\s*VQEAF_LUA_BETA_TRUST\s*'
            r'#include\s+"QeappTrustKeyLuaBeta\.h"\s*#else\b.*?#endif\b',
            dispatch_text, re.S,
        ) is not None
        add(
            'trust_dispatch', dispatch_ready,
            'QeappTrustKey.h dispatches beta builds to QeappTrustKeyLuaBeta.h',
            'Use the matching QEAPP Studio Lua beta firmware overlay',
        )

    try:
        anchor = trust_anchor(root, profile)
        trust = {
            'status': 'READY',
            'key_id': hex(anchor.key_id),
            'public_key_sha256': anchor.public_key_sha256,
            'header': anchor.header.relative_to(root).as_posix(),
        }
    except (CompatibilityError, OSError, UnicodeError) as exc:
        anchor = None
        trust = {'status': 'MISSING', 'detail': str(exc), 'action': 'Provision the matching public key and rebuild the target firmware'}

    if profile == 'lua-beta':
        try:
            production_anchor = trust_anchor(root, 'production')
            independent = (
                anchor is not None
                and anchor.key_id == 0x544c5541
                and production_anchor.key_id != anchor.key_id
                and production_anchor.public_key != anchor.public_key
            )
        except CompatibilityError:
            independent = False
        add(
            'beta_key_independence', independent,
            'Lua beta publisher key-id and P-256 point differ from production trust',
            'Provision a new beta-only publisher key',
        )

    checks['publisher_trust'] = trust
    compatible = all(value['status'] == 'READY' for value in checks.values())
    report = {
        'status': 'COMPATIBLE' if compatible else 'INCOMPATIBLE',
        'verification_scope': 'host package compatibility only; PlatformIO target build and hardware test not run',
        'firmware_repository': VQEAF_OS_REPOSITORY,
        'firmware_version': _firmware_version(root),
        'firmware_root': str(root),
        'project_type': app_type,
        'profile': profile,
        'platformio_env': env_name,
        'platformio_flags': effective_flags,
        'signer_sha256': signer_hash,
        'qeapp_contract_sha256': contract_hashes,
        'supported_app_types': list(PROFILE_TYPES[profile]),
        'checks': checks,
        'device_build': 'NOT_RUN',
        'hardware_test': 'NOT_RUN',
    }
    if anchor is not None:
        report['publisher_trust'] = trust
    return report

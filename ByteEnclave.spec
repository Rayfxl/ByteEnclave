# -*- mode: python ; coding: utf-8 -*-


a = Analysis(
    ['gui/main.py'],
    pathex=['lib'],
    binaries=[('lib/byte_enclave_python.cpython-310-x86_64-linux-gnu.so', '.')],
    datas=[],
    hiddenimports=['byte_enclave_python'],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    noarchive=False,
    optimize=0,
)
pyz = PYZ(a.pure)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.datas,
    [],
    name='ByteEnclave',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=True,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
)

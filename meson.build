project('neko-kernel-manager', 'cpp',
        version: '1.4.0',
        license: 'GPL-3.0-only',
        meson_version: '>=0.59.0',
        default_options: ['cpp_std=c++20',
                          'buildtype=debugoptimized',
                          'warning_level=3',
                          'werror=false',
                          'b_ndebug=if-release'])

is_debug_build = get_option('buildtype').startswith('debug')

cc = meson.get_compiler('cpp')
add_global_arguments('-DQT_DISABLE_DEPRECATED_BEFORE=0x050F00', language : 'cpp')

qt6 = import('qt6')
qt6_dep = dependency('qt6', modules: ['Widgets', 'Quick', 'Qml', 'Concurrent'])

qt6_resources = qt6.compile_resources(
    name: 'neko_resources',
    sources: files('resources.qrc')
)

src_files = files(
    'src/main.cpp',
    'src/kernel.cpp',
    'src/utils.cpp',
    'src/kernel-bridge.cpp',
    'src/theme-manager.cpp',
)

moc_files = qt6.compile_moc(headers: files('src/kernel-bridge.hpp', 'src/theme-manager.hpp'))

executable('neko-kernel-manager',
    sources: [src_files, qt6_resources, moc_files],
    dependencies: [qt6_dep],
    install: true,
)

install_data('neko-kernel-manager.desktop',
    install_dir: get_option('datadir') / 'applications'
)

install_data('Data/logo.png',
    rename: 'neko-kernel-manager.png',
    install_dir: get_option('datadir') / 'pixmaps'
)

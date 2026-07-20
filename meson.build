project('neko-kernel-manager', 'cpp', 'rust',
        version: '1.2.0',
        license: 'GPLv2',
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

# Common dependencies
fmt = dependency('fmt', version : ['>=11.0.0'], fallback : ['fmt', 'fmt_dep'])

# Build Rust library with Cargo
cargo = find_program('cargo')
rust_lib_name = 'config_option_lib'
rust_lib_path = meson.current_source_dir() / 'config-option-lib'
rust_target_dir = meson.current_build_dir() / 'rust'

cargo_build = custom_target('cargo-build',
    input: files('config-option-lib/src/lib.rs', 'config-option-lib/Cargo.toml'),
    output: 'lib' + rust_lib_name + '.a',
    command: [
        'sh', '-c',
        'cd ' + rust_lib_path + ' && CARGO_TARGET_DIR=' + rust_target_dir + ' ' + cargo.full_path() + ' build --release && cp ' + rust_target_dir + '/release/lib' + rust_lib_name + '.a ' + meson.current_build_dir() + '/@OUTPUT@'
    ]
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
    dependencies: [qt6_dep, fmt],
    link_with: [cargo_build],
    install: true,
)

install_data('neko-kernel-manager.desktop',
    install_dir: get_option('datadir') / 'applications'
)

install_data('Data/logo.png',
    rename: 'neko-kernel-manager.png',
    install_dir: get_option('datadir') / 'pixmaps'
)
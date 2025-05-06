# mumble-plugin-template
A template for getting started writing Mumble plugins using the standard C API.

from project root
```
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -B build
cmake --build build
```
link for easier testing
```
ln -s "$(realpath build/libwow355pa_linux_x86_64.so)" "$HOME/.local/share/Mumble/Mumble/Plugins/"
```

fix permission issues for mumble
```
sudo setcap cap_sys_ptrace=eip "$(which mumble)"
```

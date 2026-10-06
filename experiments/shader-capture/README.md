# Shader capture attempt

The initial converter interposer loaded in the game but captured no shaders. Reading a private D3DMetal cache snapshot recovered the 26 relevant containers. The same capture code worked with the synthetic shader.  
This source can write proprietary shader bytecode when loaded into a game. Define FH6_SHADER_CAPTURE_DIR to a private directory before compiling. The current workflow uses tools/extract_cache.py; captured bytecode is not part of this repository.

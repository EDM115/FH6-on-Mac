# Texture-view logging

The first logger missed the failing swizzle overload. Version 2 recorded the Depth32Float source, R32Float request and four-sample texture. That led to the guarded source-view repair in src/depth/.  
The Objective-C++ files are buildable diagnostic source. The probes include deliberately invalid cases that can abort under Metal validation. They are excluded from the normal build. Keep assertions enabled; the production trial only alters its checked source call.

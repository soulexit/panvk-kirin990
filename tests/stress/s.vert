#version 450
layout(location=0) in vec4 p;
layout(location=0) out vec4 c;
void main(){ gl_Position = vec4(p.xy, p.z, 1.0); c = vec4(fract(p.w*0.37), fract(p.w*0.11), fract(p.w*0.73), 1.0); }

#version 450
layout(set=0, binding=0) uniform samplerBuffer verts;
layout(location=0) out vec4 c;
void main(){ vec4 p = texelFetch(verts, gl_VertexIndex); gl_Position = vec4(p.xy, p.z, 1.0); c = vec4(fract(p.w*0.37), fract(p.w*0.11), fract(p.w*0.73), 1.0); }

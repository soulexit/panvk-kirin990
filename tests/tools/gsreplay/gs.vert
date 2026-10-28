#version 450
layout(location = 1) out vec4 a1;
layout(location = 2) out vec4 a2;
layout(location = 3) out vec4 a3;
layout(location = 4) out vec4 a4;
void main() {
   float i = float(gl_VertexIndex);
   gl_Position = vec4(fract(i * 0.37) * 2.0 - 1.0, fract(i * 0.61) * 2.0 - 1.0, 0.5, 1.0);
   a1 = vec4(i, 1.0, 2.0, 3.0); a2 = vec4(0.5); a3 = vec4(i * 0.01); a4 = vec4(1.0, 0.0, 0.0, 1.0);
}

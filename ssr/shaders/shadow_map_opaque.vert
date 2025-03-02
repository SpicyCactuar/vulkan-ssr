#version 460

layout(std140, set = 0, binding = 0) uniform Scene {
    mat4 V;
    mat4 P;
    mat4 VP;
    mat4 LVP;
    mat4 SLVP;
    mat4 WP;
    mat4 iP;
    mat4 C;
} scene;

layout(location = 0) in vec3 vertexPosition_wcs;

void main() {
    gl_Position = scene.LVP * vec4(vertexPosition_wcs, 1.0f);
}

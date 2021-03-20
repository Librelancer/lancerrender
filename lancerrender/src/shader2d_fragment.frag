#ifdef GL_ES
precision highp float;
#endif
in vec2 out_texcoord;
in vec2 c_pos;
in vec4 blendColor;
out vec4 out_color;
uniform sampler2D tex;
uniform float blend;
uniform bool circle;

void main()
{
    vec4 src = texture(tex, out_texcoord);
    src = mix(src, vec4(1.0,1.0,1.0, src.r), blend);
    if(circle) {
        vec2 val = c_pos - vec2(0.5);
        float r = sqrt(dot(val,val));
        float delta = fwidth(r);
        float alpha = smoothstep(0.5, 0.5 - delta, r);
        out_color = src * blendColor * vec4(1.0,1.0,1.0,alpha);
    } else {
        out_color = src * blendColor;
    }
}
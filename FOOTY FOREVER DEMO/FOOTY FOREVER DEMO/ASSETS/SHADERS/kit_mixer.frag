uniform sampler2D skinTex;
uniform vec4 skinColor;

// 8 Universal Kit Slots
uniform sampler2D tex0; uniform vec4 col0; uniform bool use0;
uniform sampler2D tex1; uniform vec4 col1; uniform bool use1;
uniform sampler2D tex2; uniform vec4 col2; uniform bool use2;
uniform sampler2D tex3; uniform vec4 col3; uniform bool use3;
uniform sampler2D tex4; uniform vec4 col4; uniform bool use4;
uniform sampler2D tex5; uniform vec4 col5; uniform bool use5;
uniform sampler2D tex6; uniform vec4 col6; uniform bool use6;
uniform sampler2D tex7; uniform vec4 col7; uniform bool use7;

void main()
{
    vec2 coord = gl_TexCoord[0].xy;
    vec4 finalColor = texture2D(skinTex, coord) * skinColor;

    // Stack them precisely in order (0 to 7)
    if (use0) { vec4 t = texture2D(tex0, coord) * col0; finalColor = mix(finalColor, t, t.a); }
    if (use1) { vec4 t = texture2D(tex1, coord) * col1; finalColor = mix(finalColor, t, t.a); }
    if (use2) { vec4 t = texture2D(tex2, coord) * col2; finalColor = mix(finalColor, t, t.a); }
    if (use3) { vec4 t = texture2D(tex3, coord) * col3; finalColor = mix(finalColor, t, t.a); }
    if (use4) { vec4 t = texture2D(tex4, coord) * col4; finalColor = mix(finalColor, t, t.a); }
    if (use5) { vec4 t = texture2D(tex5, coord) * col5; finalColor = mix(finalColor, t, t.a); }
    if (use6) { vec4 t = texture2D(tex6, coord) * col6; finalColor = mix(finalColor, t, t.a); }
    if (use7) { vec4 t = texture2D(tex7, coord) * col7; finalColor = mix(finalColor, t, t.a); }

    gl_FragColor = finalColor;
}
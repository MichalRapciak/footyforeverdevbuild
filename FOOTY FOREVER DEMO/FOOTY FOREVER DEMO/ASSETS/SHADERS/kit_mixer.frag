uniform sampler2D skinTex;
uniform vec4 skinColor;

// 15 Universal Kit Slots (Hardware Safe Maximum)
uniform sampler2D tex0; uniform vec4 col0; uniform bool use0;
uniform sampler2D tex1; uniform vec4 col1; uniform bool use1;
uniform sampler2D tex2; uniform vec4 col2; uniform bool use2;
uniform sampler2D tex3; uniform vec4 col3; uniform bool use3;
uniform sampler2D tex4; uniform vec4 col4; uniform bool use4;
uniform sampler2D tex5; uniform vec4 col5; uniform bool use5;
uniform sampler2D tex6; uniform vec4 col6; uniform bool use6;
uniform sampler2D tex7; uniform vec4 col7; uniform bool use7;
uniform sampler2D tex8; uniform vec4 col8; uniform bool use8;
uniform sampler2D tex9; uniform vec4 col9; uniform bool use9;
uniform sampler2D tex10; uniform vec4 col10; uniform bool use10;
uniform sampler2D tex11; uniform vec4 col11; uniform bool use11;
uniform sampler2D tex12; uniform vec4 col12; uniform bool use12;
uniform sampler2D tex13; uniform vec4 col13; uniform bool use13;
uniform sampler2D tex14; uniform vec4 col14; uniform bool use14;

void main()
{
    vec2 coord = gl_TexCoord[0].xy;
    vec4 finalColor = texture2D(skinTex, coord) * skinColor;

    // Stack them precisely in order (0 to 14)
    if (use0) { vec4 t = texture2D(tex0, coord) * col0; finalColor = mix(finalColor, t, t.a); }
    if (use1) { vec4 t = texture2D(tex1, coord) * col1; finalColor = mix(finalColor, t, t.a); }
    if (use2) { vec4 t = texture2D(tex2, coord) * col2; finalColor = mix(finalColor, t, t.a); }
    if (use3) { vec4 t = texture2D(tex3, coord) * col3; finalColor = mix(finalColor, t, t.a); }
    if (use4) { vec4 t = texture2D(tex4, coord) * col4; finalColor = mix(finalColor, t, t.a); }
    if (use5) { vec4 t = texture2D(tex5, coord) * col5; finalColor = mix(finalColor, t, t.a); }
    if (use6) { vec4 t = texture2D(tex6, coord) * col6; finalColor = mix(finalColor, t, t.a); }
    if (use7) { vec4 t = texture2D(tex7, coord) * col7; finalColor = mix(finalColor, t, t.a); }
    if (use8) { vec4 t = texture2D(tex8, coord) * col8; finalColor = mix(finalColor, t, t.a); }
    if (use9) { vec4 t = texture2D(tex9, coord) * col9; finalColor = mix(finalColor, t, t.a); }
    if (use10) { vec4 t = texture2D(tex10, coord) * col10; finalColor = mix(finalColor, t, t.a); }
    if (use11) { vec4 t = texture2D(tex11, coord) * col11; finalColor = mix(finalColor, t, t.a); }
    if (use12) { vec4 t = texture2D(tex12, coord) * col12; finalColor = mix(finalColor, t, t.a); }
    if (use13) { vec4 t = texture2D(tex13, coord) * col13; finalColor = mix(finalColor, t, t.a); }
    if (use14) { vec4 t = texture2D(tex14, coord) * col14; finalColor = mix(finalColor, t, t.a); }

    gl_FragColor = finalColor;
}
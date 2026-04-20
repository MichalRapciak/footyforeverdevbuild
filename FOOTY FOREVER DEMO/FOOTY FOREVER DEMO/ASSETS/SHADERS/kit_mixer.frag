// These are the 4 base textures sitting in VRAM
uniform sampler2D skinTex;
uniform sampler2D shirtTex;
uniform sampler2D shortsTex;
uniform sampler2D socksTex;

// These are the specific colors for the player we are drawing right now
uniform vec4 skinColor;
uniform vec4 shirtColor;
uniform vec4 shortsColor;
uniform vec4 socksColor;

void main()
{
    // Get the current pixel coordinate
    vec2 coord = gl_TexCoord[0].xy;

    // Grab the grayscale pixel from each texture and multiply it by our custom color
    vec4 skin   = texture2D(skinTex, coord)   * skinColor;
    vec4 shirt  = texture2D(shirtTex, coord)  * shirtColor;
    vec4 shorts = texture2D(shortsTex, coord) * shortsColor;
    vec4 socks  = texture2D(socksTex, coord)  * socksColor;

    // Stack them on top of each other!
    // The mix() function uses the alpha (transparency) of the top layer to blend it over the bottom layer.
    vec4 finalColor = skin;
    finalColor = mix(finalColor, shirt,  shirt.a);
    finalColor = mix(finalColor, shorts, shorts.a);
    finalColor = mix(finalColor, socks,  socks.a);

    // Output the final composited pixel to the monitor
    gl_FragColor = finalColor;
}
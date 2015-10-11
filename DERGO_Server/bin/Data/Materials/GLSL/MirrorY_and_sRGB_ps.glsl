#version 330

uniform sampler2D tex;

in block
{
	vec2 uv0;
} inPs;

out vec4 fragColour;

float linearToSRGB( float cl )
{
	cl = clamp( cl, 0.0, 1.0 );
	float retVal;
	retVal = cl < 0.0031308 ? (12.92 * cl) : (1.055 * pow(cl, 0.41666) - 0.055);
	
	return retVal;
}

vec3 linearToSRGB( vec3 cl )
{
	return vec3( linearToSRGB(cl.x), linearToSRGB(cl.y), linearToSRGB(cl.z) );
}

void main()
{
	vec2 uv = inPs.uv0;
	uv.y = 1.0f - uv.y;
	vec4 colour = texture( tex, uv ).xyzw;
	fragColour.xyz	= linearToSRGB( colour.zyx );
	fragColour.w	= colour.w;
}

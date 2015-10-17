
Texture2D		myTexture : register(t0);
SamplerState	mySampler : register(s0);

float linearToSRGB( float cl )
{
	cl = saturate( cl );
	float retVal;
	retVal = cl < 0.0031308 ? (12.92 * cl) : (1.055 * pow(cl, 0.41666) - 0.055);
	
	return retVal;
}

float3 linearToSRGB( float3 cl )
{
	return float3( linearToSRGB(cl.x), linearToSRGB(cl.y), linearToSRGB(cl.z) );
}

float4 main( float2 uv : TEXCOORD0 ) : SV_Target
{
	uv.y = 1.0f - uv.y;
	float4 colour = myTexture.Sample( mySampler, uv ).xyzw;
	return float4( linearToSRGB( colour.zyx ), colour.w );
}

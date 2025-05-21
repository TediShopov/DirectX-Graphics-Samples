struct VSInput
{
    float4 position : POSITION;
    float4 color : COLOR;
};

struct PSInput
{
    float4 p : SV_Position;
    
    float4 c : COLOR;
};

PSInput main(VSInput input)
{
	PSInput result;

	result.p = input.position;
	result.c = input.color;

	return result;
}

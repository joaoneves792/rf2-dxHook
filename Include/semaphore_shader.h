#ifndef __SEMAPHORE_SHADER__
#define __SEMAPHORE_SHADER__

const char * const semaphore_shader =
"struct VOut{\n"
"    float4 position : SV_POSITION;\n"
"};\n"
"\n"
"VOut VShader(float4 position : POSITION){\n"
"    VOut output;\n"
"    output.position = position;\n"
"    return output;\n"
"}\n"
"\n"
"\n"
"\n"
"float4 PShader(float4 position : SV_POSITION) : SV_TARGET{\n"
"    return float4(1.0, 0.0, 0.0, 1.0);\n"
"}\n";
#endif //__SEMAPHORE_SHADER__

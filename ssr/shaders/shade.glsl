const float PI = 3.14159265359f;
const float EPS = 1e-5f;
const vec3 dielectricReflectivity = vec3(0.04f);

// Camera is the origin of the View-space
const vec3 camera_vcs = vec3(0.0f);

// See glsl::PointLightUniform
struct PointLight {
    vec3 position_vcs;
    vec3 colour;
};

// See glsl::CameraUniform
struct Camera {
    float near;
    float far;
    vec3 position_wcs;
};

// See glsl::ShadeUniform
struct Shade {
    uint visualisationMode;
    uint pbrTerm;
    uint detailsBitfield;
    vec3 ambient;
    Camera camera;
    PointLight light;
};

// See state::VisalusationMode for specification
const uint pbrMode = 1;
const uint linearisedDepthMode = 2;
const uint normalMode = 3;
const uint positionMode = 4;
const uint viewDirectionMode = 5;
const uint lightDirectionMode = 6;
const uint roughnessMode = 7;
const uint metalnessMode = 8;
const uint reflectionDirectionMode = 9;
const uint baseColourMode = 0;

// See state::PBRTerm for specification
const uint allTerms = 1;
const uint ambientTerm = 2;
const uint diffuseTerm = 3;
const uint distributionTerm = 4;
const uint fresnelTerm = 5;
const uint geometryTerm = 6;
const uint specularTerm = 7;
const uint brdfTerm = 8;

// Result of the PBR computation containing all the terms
struct PBR {
    vec3 ambient;
    vec3 diffuse;
    float D;
    vec3 F;
    float G;
    vec3 specular;
    vec3 brdf;
    vec3 colour;
};

float saturate(float value) {
    return clamp(value, 0.0f, 1.0f);
}

vec3 fresnelSchlick(vec3 cMat, float M, float cosTheta) {
    vec3 R0 = mix(dielectricReflectivity, cMat, M);

    return R0 + (1.0f - R0) * pow(1.0f - cosTheta, 5.0f);
}

vec3 diffuseColour(vec3 cMat, float M, float cosTheta) {
    vec3 F = fresnelSchlick(cMat, M, cosTheta);

    return (cMat / PI) * (vec3(1.0f) - F) * (1.0f - M);
}

float beckmannDistribution(float alpha, float nh) {
    float alphaSquared = alpha * alpha;
    float nhSquared = nh * nh;
    float numerator = exp((nhSquared - 1.0f) / max(EPS, alphaSquared * nhSquared));
    float denominator = max(EPS, PI * alphaSquared * nhSquared * nhSquared);

    return numerator / denominator;
}

float cookTorranceMask(float nh, float nv, float nl, float vh) {
    return min(1.0f, 2.0f * min(nh * nv / vh, nh * nl / vh));
}

PBR pbr(Shade shade, vec3 normal_vcs, vec3 position_vcs, vec3 incident_vcs, vec3 cMat, vec3 E, float r, float M, float S) {
    // Prepare result
    PBR result;

    // Parameters
    vec3 cLight = shade.light.colour;
    vec3 cAmbient = shade.ambient;
    float alpha = r * r;
    vec3 n = normalize(normal_vcs);
    vec3 l = normalize(incident_vcs);
    vec3 v = normalize(camera_vcs - position_vcs);
    vec3 h = normalize(l + v);
    float nl = saturate(dot(n, l));
    float nv = saturate(dot(n, v));
    float nh = saturate(dot(n, h));
    float vh = saturate(dot(v, h));
    float lh = saturate(dot(l, h));

    // Ambient
    result.ambient = shade.ambient * cMat;

    // Diffuse
    result.diffuse = diffuseColour(cMat, M, vh);

    // Normal Distribution term
    result.D = beckmannDistribution(alpha, nh);

    // Fresnel term
    result.F = fresnelSchlick(cMat, M, lh);

    // Masking term
    result.G = cookTorranceMask(nh, nv, nl, vh);

    // Specular
    float denominator = max(EPS, 4 * nv * nl);
    result.specular = (result.D * result.F * result.G) / denominator;

    // BRDF
    result.brdf = result.diffuse + result.specular;

    // Full colour
    result.colour = E + result.ambient + S * cLight * nl * result.brdf;

    return result;
}

PBR lightPBR(Shade shade, vec3 normal_vcs, vec3 position_vcs, vec3 cMat, vec3 E, float r, float M, float S) {
    return pbr(shade, normal_vcs, position_vcs, shade.light.position_vcs - position_vcs, cMat, E, r, M, S);
}

vec3 pbrTerm(Shade shade, PBR pbr) {
    switch (shade.pbrTerm) {
        case allTerms:
            // allTerms => full equation
            return pbr.colour;
        case ambientTerm:
            return pbr.ambient;
        case diffuseTerm:
            return pbr.diffuse;
        case distributionTerm:
            return vec3(pbr.D);
        case fresnelTerm:
            return pbr.F;
        case geometryTerm:
            return vec3(pbr.G);
        case specularTerm:
            return pbr.specular;
        case brdfTerm:
            return pbr.brdf;
        default:
            return vec3(0.0f);
    }
}

float lineariseDepth(Camera camera, float depth) {
    // Linearise non-linear depth value into range [near..far]
    float near = camera.near;
    float far = camera.far;
    return (near * far) / (far - depth * (far - near));
}

vec3 reflectionDirection(vec3 normal_vcs, vec3 position_vcs) {
    vec3 I = normalize(position_vcs);
    return normalize(reflect(I, normal_vcs));
}

vec3 shade(Shade shade, float depth, vec3 normal_vcs, vec3 position_vcs, vec3 cMat, vec3 E, float r, float M, float S) {
    PBR pbr = lightPBR(shade, normal_vcs, position_vcs, cMat, E, r, M, S);
    switch (shade.visualisationMode) {
        case pbrMode:
            return pbrTerm(shade, pbr);
        case linearisedDepthMode:
            float linearDepth = lineariseDepth(shade.camera, depth);
            return vec3((linearDepth - shade.camera.near) / (shade.camera.far - shade.camera.near));
        case normalMode:
            return abs(normal_vcs);
        case positionMode:
            return position_vcs;
        case viewDirectionMode:
            // Fragment -> Camera
            return normalize(camera_vcs - position_vcs);
        case lightDirectionMode:
            // Fragment -> Light
            return normalize(shade.light.position_vcs - position_vcs);
        case roughnessMode:
            return vec3(r);
        case metalnessMode:
            return vec3(M);
        case baseColourMode:
            return cMat;
        case reflectionDirectionMode:
            return reflectionDirection(normal_vcs, position_vcs);
        default:
            return vec3(0.0f);
    }
}
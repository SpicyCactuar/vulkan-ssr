## Amazon Lumberyard Bistro Scene

The Amazon Lumberyard Bistro is sourced from NVIDIA ORCA:

https://developer.nvidia.com/orca/amazon-lumberyard-bistro

(It has been donated to NVIDIA ORCA by Amazon. See page above for details.)

License: CC-BY-NC-SA (Creative Commons Non-Commercial Share-Alike)

The model contains about 2.8M triangles.

The original model comes in the FBX format with DDS textures. It's
been converted to Wavefront OBJ from Blender. Textures were converted to PNG,
splitting roughness+metalness into separate files. See ../util/extract_rM_from_Specular.sh.
The .OBJ file is compressed with ZStandard compression (https://github.com/facebook/zstd).

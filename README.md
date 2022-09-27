# crx_packager

A crx packaging tool, but dependent on the chromium project.



##### Build Instructions:

1. The following code will be inserted at the end of the chromium/src/BUILD.gn file.

```gn
group("crx_packager"){
  data_deps = []
  data_deps += ["//crx_packager:crx_packager"]
}
```

2. Then just execute the compilation.

```bash
autoninja -C out\Release_x64 crx_packager
```

# Releases

Every release is one AdrenoTools zip that installs in all supported apps. Download from
[GitHub releases](https://github.com/soulexit/panvk-kirin990/releases). Up to 1.2.2 the
versions carried a `-winlator` suffix.

## 1.2.6 (pre-release)

```text
zip:     panvk-g76-kirin990-1.2.6.zip
sha256:  2e2ac8741d9764c64f2817d5ee60c8393c284bed252ddb8662b8b014368e6a63
armsx2:  panvk-g76-kirin990-1.2.6-armsx2.zip
sha256:  f0b81b2a2d0c45a21a45d62ef031f36393eb4b1d2da7d0add8dfbda01d17c66c
.so:     65eae575e6af3292100d91d7e26c52c1cce11fa0591f75e391ca29796ee57204
libGL_memfd.so: 32100a7900cffddcc708ea4ed1b2557404b5fa26a5c9e06d0f9cd74bbf1ef4d2
source:  Mesa 5a07217f + zenithblue 917b9b3b series + patches/0001-0057 (tree 9932fa0c18)
```

The main zip also holds `libGL_memfd.so` (Winlator OpenGL on Android 10). Driver name in app lists: `PanVK-G76-Kirin990-1.2.6`.

## 1.2.5 (pre-release)

```text
zip:     panvk-g76-kirin990-1.2.5.zip
sha256:  48801f8a6dc0c38afdd74845a4d4fde6735ca3e288bd142fb2de7085281ed7e6
armsx2:  panvk-g76-kirin990-1.2.5-armsx2.zip
sha256:  2ee9b7e9c3ac24948a28227fb504ce029bcd2a0a97693d50449e57bf39b250b8
.so:     1ad60322e5eae6237382fb996d58061b77efa098b936cb261d20883220f2ed5d
source:  Mesa 5a07217f + zenithblue 917b9b3b series + patches/0001-0056 (tree eac87c3b14)
```

Both zips hold the same driver; the `-armsx2` one names it `libvulkan_freedreno.so` for ARMSX2.

## 1.2.4 (pre-release)

```text
zip:     panvk-g76-kirin990-1.2.4.zip
sha256:  9e09250cd1b16301a5f5cf282d6da7dfee35375b1fd2346838adfd2f78c0931a
.so:     c92dcaf1fd8d91265930211f3ac1337529239d65a86b2ce2e69e38d3f3d414be
source:  Mesa 5a07217f + zenithblue 917b9b3b series + patches/0001-0054 (tree 140f694ccd)
```

## 1.2.3 (pre-release)

```text
zip:     panvk-g76-kirin990-1.2.3.zip
sha256:  44cd4bc4785ab788219e73e17fbef3f9b6351d9a90ec2e71264ac56aa830825b
.so:     8ad6819c0f8b7099e1a0119d86aa4fb21756340026f7aa12fe46ce507a49aefa
source:  Mesa 5a07217f + zenithblue 917b9b3b series + patches/0001-0045 (tree e53ed5c178)
```

## 1.2.2-winlator

```text
zip:     panvk-g76-kirin990-1.2.2-winlator.zip
sha256:  2d3b5fdd41c56d4750556693b611c84177acb4d767628b794df3aff37bc0fa18
.so:     3fedea60ab928cf5e35c1fb8516b47d2cc778a44080a60dcb1e1454ec78772fa
source:  Mesa 5a07217f + zenithblue 917b9b3b series + patches/0001-0041 (tree 40d8c0c9d8)
```

## 1.2.0-winlator

```text
zip:     panvk-g76-kirin990-1.2.0-winlator.zip
sha256:  55868e39f97b1f6eed2cb8aca61088e1733bbb74fa1ac98e54a2c97e7f270a57
.so:     1f5d8e7f039eb4cb461445a4504ee5539f2631f94766812a8144399cdfbf262f
source:  Mesa 5a07217f + zenithblue 917b9b3b series + patches/0001-0036 (tree ded07de903)
```

## 1.1.0-winlator

CTS fixes (, tree 2b001a2bfd) ; not published separately: its changes
are in 1.2.0.

## 1.0.0-winlator

```text
zip:     panvk-g76-kirin990-1.0.0-winlator.zip
sha256:  2123e49b8958230d89930c03dbc971fa2b2285f1e1ecbcfc76d387be73cdb728
.so:     50e9d366cb064cd2686c60f95d5dc83cf94becaf6261ad883129c879b9528d49
source:  Mesa 5a07217f + zenithblue 917b9b3b series + patches/0001-0018 (tree 60571fd11b)
```

The zip is a standard AdrenoTools package: one `libvulkan_panfrost.so` (static libc++,
no other libraries) and `meta.json` (`minApi` 29).

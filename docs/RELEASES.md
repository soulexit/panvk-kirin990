# Releases

Every release is one AdrenoTools zip that installs in all supported apps. Download from
[GitHub releases](https://github.com/soulexit/panvk-kirin990/releases). Up to 1.2.2 the
versions carried a `-winlator` suffix.

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

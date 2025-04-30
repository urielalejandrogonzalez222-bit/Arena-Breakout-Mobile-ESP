## Arena Breakout Mobile

ESP For Arena Breakout Mobile, Made by Fakekey

## **Note**:

- Only for Educational Purpose.
- This project is NOT affiliated or approved by TENCENT, Level Infinite or Arena Breakout Mobile

##

```shell
cd $(echo $(pm path com.vng.mf.liteuamo | awk 'NR==1') | sed 's/package://; s/\/base.apk//')/lib/arm64
```

```shell
mv /sdcard/libmono.so . && chmod 755 libmono.so
```

## Another approach

You might wanna take a look at [Agnoster](https://github.com/fakekey/Agnoster) and [AgnosterAPK](https://github.com/fakekey/AgnosterAPK). They create new surface to render overlay instead of hooking eglSwapBuffers. Applied APKKiller too, maybe bypass some integrity checks (hope)

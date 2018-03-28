package ywl5320.com.soundtouchdemo;

/**
 * Created by hlwky001 on 2018/3/19.
 */

public class SoundTouch {

    static {
        System.loadLibrary("native-lib");
    }

    public native void playAudioByOpenSL_pcm(String pamPath);

}

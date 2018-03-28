package ywl5320.com.soundtouchdemo;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    SoundTouch soundTouch;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        soundTouch = new SoundTouch();
    }

    public void play(View view) {
        soundTouch.playAudioByOpenSL_pcm("/mnt/shared/Other/mydream.pcm");
    }
}

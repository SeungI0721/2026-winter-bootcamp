package com.example.rc_controller;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.MotionEvent;
import android.widget.Button;
import android.widget.Toast;
import android.annotation.SuppressLint;


import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import java.io.IOException;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Set;
import java.util.UUID;

public class MainActivity extends AppCompatActivity {

    // SPP(시리얼) UUID (HC-05/HC-06)
    private static final UUID SPP_UUID =
            UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

    private static final String TARGET_DEVICE_NAME = "edu14";

    private BluetoothAdapter btAdapter;
    private BluetoothSocket btSocket;
    private OutputStream out;

    private Button btnConnect, btnMode;
    private Button btnForward, btnBack, btnLeft, btnRight, btnStop;

    private boolean isConnected = false;
    private boolean isManualMode = true; // UI 초기값 "MODE: MANUAL"과 맞춤

    // 버튼 누르는 동안 너무 자주 보내면 과부하 생길 수 있어서 최소 간격
    private static final long REPEAT_INTERVAL_MS = 80;
    private long lastSentAt = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        btAdapter = BluetoothAdapter.getDefaultAdapter();

        // XML ID 매칭
        btnConnect = findViewById(R.id.btnConnect);
        btnMode = findViewById(R.id.btnMode);

        btnForward = findViewById(R.id.btnForward);
        btnBack = findViewById(R.id.btnBack);
        btnLeft = findViewById(R.id.btnLeft);
        btnRight = findViewById(R.id.btnRight);
        btnStop = findViewById(R.id.btnStop);

        btnConnect.setOnClickListener(v -> connectToCar());

        // MODE 버튼: MANUAL <-> AUTO 토글
        btnMode.setOnClickListener(v -> {
            if (!isConnected) {
                toast("Not connected");
                return;
            }
            isManualMode = !isManualMode;
            if (isManualMode) {
                btnMode.setText("MODE: MANUAL");
                // 아두이노: manual 모드 진입
                sendCmdString("manual_");  // "manual" + '_' + '\n'
            } else {
                btnMode.setText("MODE: AUTO");
                // 아두이노: auto 모드 진입(네 코드 기준 start가 autoMode=true)
                sendCmdString("start_");
            }
        });

        // 방향키: 누르는 동안 움직이고, 손 떼면 정지(S)
        setHoldToSend(btnForward, 'F');
        setHoldToSend(btnBack, 'B');
        setHoldToSend(btnLeft, 'L');
        setHoldToSend(btnRight, 'R');

        // 중앙 STOP: 확실한 정지 = stop_ + S
        btnStop.setOnClickListener(v -> {
            if (!isConnected) {
                toast("Not connected");
                return;
            }
            sendCmdString("stop_");
            sendChar('S'); // 수동에서도 확실히 멈추게
        });

        // 연결 전에는 조종 버튼 비활성
        setControlsEnabled(false);
        btnMode.setText("MODE: MANUAL");
    }

    private void setControlsEnabled(boolean enabled) {
        btnMode.setEnabled(enabled);
        btnForward.setEnabled(enabled);
        btnBack.setEnabled(enabled);
        btnLeft.setEnabled(enabled);
        btnRight.setEnabled(enabled);
        btnStop.setEnabled(enabled);
    }

    /* ---------------------------
     * Bluetooth 연결
     * --------------------------- */
    private void connectToCar() {
        if (btAdapter == null) {
            toast("Bluetooth not supported");
            return;
        }
        if (!btAdapter.isEnabled()) {
            toast("Bluetooth is OFF. Turn it ON first.");
            return;
        }

        // Android 12+ 권한
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT)
                    != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this,
                        new String[]{Manifest.permission.BLUETOOTH_CONNECT}, 100);
                return;
            }
        }

        BluetoothDevice device = findPairedDeviceByName(TARGET_DEVICE_NAME);
        if (device == null) {
            toast("Paired device not found: " + TARGET_DEVICE_NAME + "\n(설정에서 먼저 페어링하세요)");
            return;
        }

        toast("Connecting...");
        new Thread(() -> {
            try {
                closeConnection();

                btSocket = device.createRfcommSocketToServiceRecord(SPP_UUID);
                btSocket.connect();
                out = btSocket.getOutputStream();

                runOnUiThread(() -> {
                    isConnected = true;
                    toast("Connected!");
                    setControlsEnabled(true);

                    // 연결되면 기본은 MANUAL로 둔다(원하면 AUTO로 바꿔도 됨)
                    isManualMode = true;
                    btnMode.setText("MODE: MANUAL");
                    sendCmdString("manual_");
                });

            } catch (IOException e) {
                runOnUiThread(() -> {
                    isConnected = false;
                    toast("Connect failed: " + e.getMessage());
                    setControlsEnabled(false);
                });
                closeConnection();
            }
        }).start();
    }

    @SuppressLint("MissingPermission")
    private BluetoothDevice findPairedDeviceByName(String name) {
        Set<BluetoothDevice> bonded = btAdapter.getBondedDevices();
        if (bonded == null) return null;
        for (BluetoothDevice d : bonded) {
            String dn = d.getName();
            if (dn != null && dn.equals(name)) return d;
        }
        return null;
    }

    /* ---------------------------
     * 전송 유틸
     * --------------------------- */

    // 문자열 명령 전송: 예) "start_" -> "start_\n"
    private void sendCmdString(String payloadWithUnderscore) {
        if (out == null) return;
        try {
            String msg = payloadWithUnderscore + "\n";
            out.write(msg.getBytes(StandardCharsets.UTF_8));
            out.flush();
            //toast("Sent: " + payloadWithUnderscore);
        } catch (IOException e) {
            onIoError(e);
        }
    }

    // 1글자 명령 전송: 예) 'F' -> "F\n"
    private void sendChar(char c) {
        if (out == null) return;
        try {
            String msg = c + "\n";
            out.write(msg.getBytes(StandardCharsets.UTF_8));
            out.flush();
        } catch (IOException e) {
            onIoError(e);
        }
    }

    private void onIoError(IOException e) {
        runOnUiThread(() -> {
            toast("Bluetooth error: " + e.getMessage());
            isConnected = false;
            setControlsEnabled(false);
        });
        closeConnection();
    }

    /* ---------------------------
     * 버튼 누르는 동안 전송(hold)
     * --------------------------- */
    private void setHoldToSend(Button btn, char moveChar) {
        btn.setOnTouchListener((v, event) -> {
            if (!isConnected) return false;

            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    // 수동 모드가 아니면 수동으로 바꿔서 조종하게(편의)
                    if (!isManualMode) {
                        isManualMode = true;
                        btnMode.setText("MODE: MANUAL");
                        sendCmdString("manual_");
                    }
                    // 첫 입력 즉시 전송
                    sendChar(moveChar);
                    lastSentAt = SystemClock.elapsedRealtime();
                    return true;

                case MotionEvent.ACTION_MOVE:
                    // 길게 누를 때 주기적으로 재전송(안드로이드가 MOVE를 계속 주는 편)
                    long now = SystemClock.elapsedRealtime();
                    if (now - lastSentAt >= REPEAT_INTERVAL_MS) {
                        sendChar(moveChar);
                        lastSentAt = now;
                    }
                    return true;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    // 손 떼면 정지
                    sendChar('S');
                    return true;
            }
            return false;
        });
    }

    private void closeConnection() {
        try { if (out != null) out.close(); } catch (Exception ignored) {}
        try { if (btSocket != null) btSocket.close(); } catch (Exception ignored) {}
        out = null;
        btSocket = null;
    }

    private void toast(String s) {
        Toast.makeText(this, s, Toast.LENGTH_SHORT).show();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        closeConnection();
    }
}

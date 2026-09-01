package org.libsdl.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.List;

import com.spaghettify.mariokart64.R;

public class ConfigActivity extends Activity {

    private static final String TAG = "MK64Config";
    public static final String PREFS_NAME = "MK64_Settings";

    // Tabs & Panels
    private Button tabControls, tabGameplay, tabTracks, tabSound, tabMultiplayer, tabRaw;
    private View panelControls, panelGameplay, panelTracks, panelSound, panelMultiplayer, panelRaw;
    private Button currentTabButton;
    private View currentPanel;

    // Header buttons
    private Button btnSave, btnClose, btnRestoreDefaults, btnReloadRaw;

    // Multiplayer tab widgets
    private TextView tvNetHostIp;
    private Button btnCopyHostIp, btnStartHost, btnJoinGame;
    private EditText etHostPort, etJoinIp, etJoinPort;

    // Controls tab widgets
    private SeekBar sbOpacity;
    private TextView tvOpacityVal;
    private Spinner spButtonSize;
    private CheckBox cbCButtons, cbExtraZ, cbFloatingAnalog;

    // Gameplay tab widgets
    private Spinner spCustomCC;
    private CheckBox cbSkipIntro, cbFreecam, cbDebugMode, cbShowVersion;

    // Tracks tab widgets
    private CheckBox cbDisableLod, cbNoCulling;
    private Spinner spNumTrains, spNumTraffic;

    // Sound tab widgets
    private SeekBar sbVolMaster, sbVolMusic, sbVolSfx;
    private TextView tvVolMasterVal, tvVolMusicVal, tvVolSfxVal;

    // Raw JSON tab
    private EditText etRawJson;

    private SharedPreferences prefs;
    private JSONObject configJson;
    private File configFile;

    private static final List<String> BUTTON_SIZES = Arrays.asList("Pequeno (80%)", "Médio (100% - Padrão)", "Grande (120%)", "Extra Grande (140%)");
    private static final List<String> CC_OPTIONS = Arrays.asList("Padrão (50cc/100cc/150cc)", "50cc", "100cc", "150cc", "200cc (Rápido)", "300cc (Insano)");
    private static final List<String> TRAIN_OPTIONS = Arrays.asList("1 Trem", "2 Trens (Padrão)", "3 Trens", "4 Trens", "5 Trens");
    private static final List<String> TRAFFIC_OPTIONS = Arrays.asList("Baixo (3 veículos)", "Médio / Padrão (7 veículos)", "Alto (12 veículos)", "Caos Total (20 veículos)");

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
        setContentView(R.layout.activity_config);

        prefs = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);

        initViews();
        setupTabs();
        setupSpinners();
        setupSeekBars();
        setupMultiplayer();
        loadSettings();
    }

    private void initViews() {
        tabControls = findViewById(R.id.tab_controls);
        tabGameplay = findViewById(R.id.tab_gameplay);
        tabTracks = findViewById(R.id.tab_tracks);
        tabSound = findViewById(R.id.tab_sound);
        tabMultiplayer = findViewById(R.id.tab_multiplayer);
        tabRaw = findViewById(R.id.tab_raw);

        panelControls = findViewById(R.id.panel_controls);
        panelGameplay = findViewById(R.id.panel_gameplay);
        panelTracks = findViewById(R.id.panel_tracks);
        panelSound = findViewById(R.id.panel_sound);
        panelMultiplayer = findViewById(R.id.panel_multiplayer);
        panelRaw = findViewById(R.id.panel_raw);

        btnSave = findViewById(R.id.btn_save);
        btnClose = findViewById(R.id.btn_close);
        btnRestoreDefaults = findViewById(R.id.btn_restore_defaults);
        btnReloadRaw = findViewById(R.id.btn_reload_raw);

        // Multiplayer widgets
        tvNetHostIp = findViewById(R.id.tv_net_host_ip);
        btnCopyHostIp = findViewById(R.id.btn_copy_host_ip);
        btnStartHost = findViewById(R.id.btn_start_host);
        etHostPort = findViewById(R.id.et_host_port);
        etJoinIp = findViewById(R.id.et_join_ip);
        etJoinPort = findViewById(R.id.et_join_port);
        btnJoinGame = findViewById(R.id.btn_join_game);

        sbOpacity = findViewById(R.id.sb_opacity);
        tvOpacityVal = findViewById(R.id.tv_opacity_val);
        spButtonSize = findViewById(R.id.sp_button_size);
        cbCButtons = findViewById(R.id.cb_c_buttons);
        cbExtraZ = findViewById(R.id.cb_extra_z);
        cbFloatingAnalog = findViewById(R.id.cb_floating_analog);

        spCustomCC = findViewById(R.id.sp_custom_cc);
        cbSkipIntro = findViewById(R.id.cb_skip_intro);
        cbFreecam = findViewById(R.id.cb_freecam);
        cbDebugMode = findViewById(R.id.cb_debug_mode);
        cbShowVersion = findViewById(R.id.cb_show_version);

        cbDisableLod = findViewById(R.id.cb_disable_lod);
        cbNoCulling = findViewById(R.id.cb_no_culling);
        spNumTrains = findViewById(R.id.sp_num_trains);
        spNumTraffic = findViewById(R.id.sp_num_traffic);

        sbVolMaster = findViewById(R.id.sb_vol_master);
        sbVolMusic = findViewById(R.id.sb_vol_music);
        sbVolSfx = findViewById(R.id.sb_vol_sfx);
        tvVolMasterVal = findViewById(R.id.tv_vol_master_val);
        tvVolMusicVal = findViewById(R.id.tv_vol_music_val);
        tvVolSfxVal = findViewById(R.id.tv_vol_sfx_val);

        etRawJson = findViewById(R.id.et_raw_json);

        btnSave.setOnClickListener(v -> saveSettings());
        btnClose.setOnClickListener(v -> finish());
        btnRestoreDefaults.setOnClickListener(v -> confirmRestoreDefaults());
        btnReloadRaw.setOnClickListener(v -> loadConfigFile());
    }

    private void setupTabs() {
        currentTabButton = tabControls;
        currentPanel = panelControls;

        View.OnClickListener tabListener = v -> {
            if (v == currentTabButton) return;

            // Deactivate current
            if (currentTabButton != null) {
                currentTabButton.setBackgroundResource(R.drawable.btn_tab_inactive);
                currentTabButton.setTextColor(Color.parseColor("#8B9BB4"));
            }
            if (currentPanel != null) {
                currentPanel.setVisibility(View.GONE);
            }

            // Activate new
            currentTabButton = (Button) v;
            currentTabButton.setBackgroundResource(R.drawable.btn_tab_active);
            currentTabButton.setTextColor(Color.parseColor("#FFC107"));

            if (v == tabControls) currentPanel = panelControls;
            else if (v == tabGameplay) currentPanel = panelGameplay;
            else if (v == tabTracks) currentPanel = panelTracks;
            else if (v == tabSound) currentPanel = panelSound;
            else if (v == tabMultiplayer) {
                currentPanel = panelMultiplayer;
                updateMultiplayerInfo();
            }
            else if (v == tabRaw) {
                currentPanel = panelRaw;
                updateRawJsonText();
            }

            if (currentPanel != null) {
                currentPanel.setVisibility(View.VISIBLE);
            }
        };

        tabControls.setOnClickListener(tabListener);
        tabGameplay.setOnClickListener(tabListener);
        tabTracks.setOnClickListener(tabListener);
        tabSound.setOnClickListener(tabListener);
        tabMultiplayer.setOnClickListener(tabListener);
        tabRaw.setOnClickListener(tabListener);
    }

    private void setupMultiplayer() {
        String lastIp = prefs.getString("net_last_ip", "192.168.43.1");
        int lastPort = prefs.getInt("net_last_port", 27100);
        if (etJoinIp != null) etJoinIp.setText(lastIp);
        if (etJoinPort != null) etJoinPort.setText(String.valueOf(lastPort));

        if (btnCopyHostIp != null) {
            btnCopyHostIp.setOnClickListener(v -> {
                String primaryIp = getPrimaryIpAddress();
                android.content.ClipboardManager clipboard = (android.content.ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
                if (clipboard != null) {
                    android.content.ClipData clip = android.content.ClipData.newPlainText("IP Mario Kart 64", primaryIp);
                    clipboard.setPrimaryClip(clip);
                    Toast.makeText(this, "IP " + primaryIp + " copiado para a área de transferência!", Toast.LENGTH_SHORT).show();
                }
            });
        }

        if (btnStartHost != null) {
            btnStartHost.setOnClickListener(v -> {
                int port = 27100;
                try {
                    port = Integer.parseInt(etHostPort.getText().toString().trim());
                } catch (Exception ignored) {}

                Intent intent = new Intent(this, SDLActivity.class);
                intent.putExtra("extra_net_mode", 1); // 1 = Host
                intent.putExtra("extra_net_port", port);
                intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                startActivity(intent);
                finish();
            });
        }

        if (btnJoinGame != null) {
            btnJoinGame.setOnClickListener(v -> {
                String ip = etJoinIp.getText().toString().trim();
                if (ip.isEmpty()) ip = "127.0.0.1";

                int port = 27100;
                try {
                    port = Integer.parseInt(etJoinPort.getText().toString().trim());
                } catch (Exception ignored) {}

                prefs.edit()
                        .putString("net_last_ip", ip)
                        .putInt("net_last_port", port)
                        .apply();

                Intent intent = new Intent(this, SDLActivity.class);
                intent.putExtra("extra_net_mode", 2); // 2 = Join
                intent.putExtra("extra_net_ip", ip);
                intent.putExtra("extra_net_port", port);
                intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                startActivity(intent);
                finish();
            });
        }
    }

    private void updateMultiplayerInfo() {
        if (tvNetHostIp != null) {
            tvNetHostIp.setText(getDeviceIpSummary());
        }
    }

    private String getDeviceIpSummary() {
        StringBuilder sb = new StringBuilder();
        try {
            java.util.Enumeration<java.net.NetworkInterface> interfaces = java.net.NetworkInterface.getNetworkInterfaces();
            while (interfaces != null && interfaces.hasMoreElements()) {
                java.net.NetworkInterface iface = interfaces.nextElement();
                if (iface.isLoopback() || !iface.isUp()) continue;
                String name = iface.getName().toLowerCase();
                String displayName = iface.getDisplayName().toLowerCase();

                String label = "Rede Local";
                if (name.contains("wlan") || displayName.contains("wlan") || displayName.contains("wi-fi")) {
                    label = "Wi-Fi";
                } else if (name.contains("ap") || name.contains("rndis") || name.contains("tether")) {
                    label = "Ponto de Acesso (Hotspot)";
                } else if (name.contains("eth")) {
                    label = "Ethernet";
                } else if (name.contains("tun") || name.contains("zt") || name.contains("tailscale")) {
                    label = "VPN / ZeroTier";
                }

                java.util.Enumeration<java.net.InetAddress> addresses = iface.getInetAddresses();
                while (addresses.hasMoreElements()) {
                    java.net.InetAddress addr = addresses.nextElement();
                    if (!addr.isLoopbackAddress() && addr instanceof java.net.Inet4Address) {
                        if (sb.length() > 0) sb.append("\n");
                        sb.append(label).append(" (").append(iface.getName()).append("): ").append(addr.getHostAddress());
                    }
                }
            }
        } catch (Exception ignored) {}

        if (sb.length() == 0) {
            return "127.0.0.1 (Sem conexão Wi-Fi/Hotspot ativa)";
        }
        return sb.toString();
    }

    private String getPrimaryIpAddress() {
        try {
            java.util.Enumeration<java.net.NetworkInterface> interfaces = java.net.NetworkInterface.getNetworkInterfaces();
            while (interfaces != null && interfaces.hasMoreElements()) {
                java.net.NetworkInterface iface = interfaces.nextElement();
                if (iface.isLoopback() || !iface.isUp()) continue;
                java.util.Enumeration<java.net.InetAddress> addresses = iface.getInetAddresses();
                while (addresses.hasMoreElements()) {
                    java.net.InetAddress addr = addresses.nextElement();
                    if (!addr.isLoopbackAddress() && addr instanceof java.net.Inet4Address) {
                        return addr.getHostAddress();
                    }
                }
            }
        } catch (Exception ignored) {}
        return "127.0.0.1";
    }

    private void setupSpinners() {
        ArrayAdapter<String> adapterSizes = new ArrayAdapter<>(this, R.layout.custom_spinner_item, BUTTON_SIZES);
        adapterSizes.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spButtonSize.setAdapter(adapterSizes);

        ArrayAdapter<String> adapterCC = new ArrayAdapter<>(this, R.layout.custom_spinner_item, CC_OPTIONS);
        adapterCC.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spCustomCC.setAdapter(adapterCC);

        ArrayAdapter<String> adapterTrains = new ArrayAdapter<>(this, R.layout.custom_spinner_item, TRAIN_OPTIONS);
        adapterTrains.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spNumTrains.setAdapter(adapterTrains);

        ArrayAdapter<String> adapterTraffic = new ArrayAdapter<>(this, R.layout.custom_spinner_item, TRAFFIC_OPTIONS);
        adapterTraffic.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spNumTraffic.setAdapter(adapterTraffic);
    }

    private void setupSeekBars() {
        sbOpacity.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (progress < 20) progress = 20; // Minimum 20%
                tvOpacityVal.setText(progress + "%");
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        sbVolMaster.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvVolMasterVal.setText(progress + "%");
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        sbVolMusic.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvVolMusicVal.setText(progress + "%");
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        sbVolSfx.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                tvVolSfxVal.setText(progress + "%");
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });
    }

    private void loadSettings() {
        // 1. SharedPreferences (UI Controls)
        int opacity = prefs.getInt("ctrl_opacity", 70);
        sbOpacity.setProgress(opacity);
        tvOpacityVal.setText(opacity + "%");

        int sizeIdx = prefs.getInt("ctrl_size_idx", 1);
        if (sizeIdx >= 0 && sizeIdx < BUTTON_SIZES.size()) spButtonSize.setSelection(sizeIdx);

        cbCButtons.setChecked(prefs.getBoolean("ctrl_c_buttons", true));
        cbExtraZ.setChecked(prefs.getBoolean("ctrl_extra_z", true));
        cbFloatingAnalog.setChecked(prefs.getBoolean("ctrl_floating_analog", true));

        // 2. Load JSON config file
        loadConfigFile();
    }

    private void loadConfigFile() {
        File mk64Dir = new File(Environment.getExternalStorageDirectory(), "MK64");
        configFile = new File(mk64Dir, "spaghettify.cfg.json");

        if (!configFile.exists()) {
            // Check fallback
            File fallback = new File(getExternalFilesDir(null), "spaghettify.cfg.json");
            if (fallback.exists()) configFile = fallback;
        }

        try {
            if (configFile.exists()) {
                FileInputStream fis = new FileInputStream(configFile);
                byte[] data = new byte[(int) configFile.length()];
                fis.read(data);
                fis.close();
                String jsonStr = new String(data, StandardCharsets.UTF_8);
                configJson = new JSONObject(jsonStr);
            } else {
                configJson = new JSONObject();
            }

            JSONObject cvars = configJson.optJSONObject("CVars");
            if (cvars == null) cvars = new JSONObject();

            // Gameplay
            int customCc = cvars.optInt("gCustomCC", 150);
            boolean enableCustomCc = cvars.optInt("gEnableCustomCC", 0) == 1;
            if (!enableCustomCc) spCustomCC.setSelection(0);
            else if (customCc <= 50) spCustomCC.setSelection(1);
            else if (customCc <= 100) spCustomCC.setSelection(2);
            else if (customCc <= 150) spCustomCC.setSelection(3);
            else if (customCc <= 200) spCustomCC.setSelection(4);
            else spCustomCC.setSelection(5);

            cbSkipIntro.setChecked(cvars.optInt("gSkipIntro", 0) == 1);
            cbFreecam.setChecked(cvars.optInt("gFreecam", 0) == 1);
            cbDebugMode.setChecked(cvars.optInt("gEnableDebugMode", 0) == 1);
            cbShowVersion.setChecked(cvars.optInt("gShowSpaghettiVersion", 1) == 1);

            // Tracks
            cbDisableLod.setChecked(cvars.optInt("gDisableLod", 1) == 1);
            cbNoCulling.setChecked(cvars.optInt("gNoCulling", 0) == 1);

            int numTrains = cvars.optInt("gNumTrains", 2);
            spNumTrains.setSelection(Math.max(0, Math.min(numTrains - 1, TRAIN_OPTIONS.size() - 1)));

            int numCars = cvars.optInt("gNumCars", 7);
            if (numCars <= 3) spNumTraffic.setSelection(0);
            else if (numCars <= 7) spNumTraffic.setSelection(1);
            else if (numCars <= 12) spNumTraffic.setSelection(2);
            else spNumTraffic.setSelection(3);

            // Audio
            double volMaster = cvars.optDouble("gGameMasterVolume", 1.0);
            double volMusic = cvars.optDouble("gMainMusicVolume", 1.0);
            double volSfx = cvars.optDouble("gSFXMusicVolume", 1.0);

            sbVolMaster.setProgress((int) (volMaster * 100));
            sbVolMusic.setProgress((int) (volMusic * 100));
            sbVolSfx.setProgress((int) (volSfx * 100));

            updateRawJsonText();

        } catch (Exception e) {
            Log.e(TAG, "Erro ao carregar spaghettify.cfg.json", e);
            configJson = new JSONObject();
        }
    }

    private void updateRawJsonText() {
        if (configJson != null) {
            try {
                etRawJson.setText(configJson.toString(2));
            } catch (Exception e) {
                etRawJson.setText(configJson.toString());
            }
        }
    }

    private void saveSettings() {
        // 1. Save UI Controls to SharedPreferences
        int opacity = sbOpacity.getProgress();
        if (opacity < 20) opacity = 20;
        int sizeIdx = spButtonSize.getSelectedItemPosition();

        prefs.edit()
                .putInt("ctrl_opacity", opacity)
                .putInt("ctrl_size_idx", sizeIdx)
                .putBoolean("ctrl_c_buttons", cbCButtons.isChecked())
                .putBoolean("ctrl_extra_z", cbExtraZ.isChecked())
                .putBoolean("ctrl_floating_analog", cbFloatingAnalog.isChecked())
                .apply();

        // 2. Update JSON CVars
        try {
            if (currentPanel == panelRaw) {
                // If on RAW tab, parse directly from text edit
                String rawText = etRawJson.getText().toString().trim();
                if (!rawText.isEmpty()) {
                    configJson = new JSONObject(rawText);
                }
            } else {
                JSONObject cvars = configJson.optJSONObject("CVars");
                if (cvars == null) {
                    cvars = new JSONObject();
                    configJson.put("CVars", cvars);
                }

                // CC selection
                int ccIdx = spCustomCC.getSelectedItemPosition();
                if (ccIdx == 0) {
                    cvars.put("gEnableCustomCC", 0);
                } else {
                    cvars.put("gEnableCustomCC", 1);
                    int[] ccValues = {0, 50, 100, 150, 200, 300};
                    cvars.put("gCustomCC", ccValues[ccIdx]);
                }

                cvars.put("gSkipIntro", cbSkipIntro.isChecked() ? 1 : 0);
                cvars.put("gFreecam", cbFreecam.isChecked() ? 1 : 0);
                cvars.put("gEnableDebugMode", cbDebugMode.isChecked() ? 1 : 0);
                cvars.put("gShowSpaghettiVersion", cbShowVersion.isChecked() ? 1 : 0);

                cvars.put("gDisableLod", cbDisableLod.isChecked() ? 1 : 0);
                cvars.put("gNoCulling", cbNoCulling.isChecked() ? 1 : 0);

                int trains = spNumTrains.getSelectedItemPosition() + 1;
                cvars.put("gNumTrains", trains);

                int trafficIdx = spNumTraffic.getSelectedItemPosition();
                int[] carVals = {3, 7, 12, 20};
                cvars.put("gNumCars", carVals[trafficIdx]);
                cvars.put("gNumTrucks", carVals[trafficIdx]);
                cvars.put("gNumBuses", carVals[trafficIdx]);
                cvars.put("gNumTankerTrucks", carVals[trafficIdx]);

                cvars.put("gGameMasterVolume", sbVolMaster.getProgress() / 100.0);
                cvars.put("gMainMusicVolume", sbVolMusic.getProgress() / 100.0);
                cvars.put("gSFXMusicVolume", sbVolSfx.getProgress() / 100.0);
                cvars.put("gEnvironmentVolume", sbVolMusic.getProgress() / 100.0);
            }

            // Write to disk
            if (configFile != null) {
                if (!configFile.getParentFile().exists()) {
                    configFile.getParentFile().mkdirs();
                }
                FileOutputStream fos = new FileOutputStream(configFile);
                fos.write(configJson.toString(2).getBytes(StandardCharsets.UTF_8));
                fos.flush();
                fos.close();
            }

            Toast.makeText(this, "Configurações salvas com sucesso!", Toast.LENGTH_SHORT).show();
            finish();

        } catch (Exception e) {
            Log.e(TAG, "Erro ao salvar configuracoes", e);
            Toast.makeText(this, "Erro ao salvar: " + e.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    private void confirmRestoreDefaults() {
        new AlertDialog.Builder(this)
                .setTitle("Restaurar Padrões")
                .setMessage("Deseja restaurar todas as configurações para o padrão original?")
                .setPositiveButton("Sim", (dialog, which) -> {
                    sbOpacity.setProgress(70);
                    spButtonSize.setSelection(1);
                    cbCButtons.setChecked(true);
                    cbExtraZ.setChecked(true);
                    cbFloatingAnalog.setChecked(true);

                    spCustomCC.setSelection(0);
                    cbSkipIntro.setChecked(false);
                    cbFreecam.setChecked(false);
                    cbDebugMode.setChecked(false);
                    cbShowVersion.setChecked(true);

                    cbDisableLod.setChecked(true);
                    cbNoCulling.setChecked(false);
                    spNumTrains.setSelection(1);
                    spNumTraffic.setSelection(1);

                    sbVolMaster.setProgress(100);
                    sbVolMusic.setProgress(100);
                    sbVolSfx.setProgress(100);

                    Toast.makeText(this, "Valores padrão restaurados.", Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton("Cancelar", null)
                .show();
    }
}

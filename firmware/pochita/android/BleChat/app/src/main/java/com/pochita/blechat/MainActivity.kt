package com.pochita.blechat

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.pm.PackageManager
import android.location.LocationManager
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.View
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ListView
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import java.util.UUID

/**
 * BLE Chat client for POCHITA OS "POCHITA-CHAT".
 *
 * Protocol (mirrors src/bitchat.h):
 *  - Service:   6e400001-b5a3-f393-e0a9-e50e24dcca9e
 *  - RX write:  6e400002-... (phone -> ESP32)
 *  - TX notify: 6e400003-... (ESP32 -> phone)
 *  - Short messages (<= 18 bytes): raw UTF-8 text.
 *  - Long messages: frames [0x01][S|C|E][data], each frame <= 18 bytes of payload.
 *    S = start, C = continue, E = end. A bare 0x01 'M' frame = one complete message.
 *  - Incoming raw text: \r is dropped, \n becomes a space.
 */
class MainActivity : AppCompatActivity() {

    companion object {
        private val SERVICE_UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
        private val RX_UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
        private val TX_UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
        private val CCCD_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        private const val CTRL = 0x01
        private const val CHUNK = 18
        private const val FRAME_FLUSH_MS = 600L
        private const val TARGET_NAME = "POCHITA-CHAT"
        private const val DEVICE_TAG = "POCHITA"
    }

    private var bluetoothManager: BluetoothManager? = null
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var scanner: BluetoothLeScanner? = null

    private var statusTv: TextView? = null
    private var chatTv: TextView? = null
    private var chatScroll: ScrollView? = null
    private var inputEt: EditText? = null
    private var deviceList: ListView? = null
    private var devicePanel: LinearLayout? = null
    private var deviceAdapter: ArrayAdapter<String>? = null

    private val devices = mutableMapOf<String, BluetoothDevice>()

    private var gatt: BluetoothGatt? = null
    private var rxChar: BluetoothGattCharacteristic? = null
    private var txChar: BluetoothGattCharacteristic? = null
    private var connected = false
    private var scanning = false
    private var pendingDevice: BluetoothDevice? = null
    private var connectAttempts = 0
    private var discoveryAttempts = 0

    private val rxBuf = StringBuilder()
    private val mainHandler = Handler(Looper.getMainLooper())
    private val frameFlush = Runnable {
        if (rxBuf.isNotEmpty()) {
            deliver(rxBuf.toString())
            rxBuf.clear()
        }
    }

    // Connects to the first matching device once a scan finds it.
    private val autoConnect: Runnable = Runnable {
        if (!connected && gatt == null && pendingDevice == null) {
            devices.values.firstOrNull()?.let { connect(it) }
        }
    }

    private val retryConnect: Runnable = Runnable {
        val dev = pendingDevice ?: return@Runnable
        if (!connected && gatt == null) {
            setStatus("Đang thử lại kết nối ${dev.name ?: dev.address}...")
            try {
                gatt = dev.connectGatt(this, true, gattCallback)
            } catch (e: Exception) {
                Log.e("BLE", "retry connect", e)
                setStatus("Thử lại thất bại. Đang quét...")
                pendingDevice = null
                startScan()
            }
        }
    }

    private val retryDiscover: Runnable = Runnable {
        if (connected && gatt != null) {
            try {
                gatt!!.discoverServices()
            } catch (e: Exception) {
                Log.e("BLE", "discover retry", e)
            }
        }
    }

    private val permLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { grant ->
        if (grant.values.all { it }) {
            startScan()
        } else {
            setStatus("Cần cấp quyền Bluetooth/Location để quét")
        }
    }

    // ---- scan ---------------------------------------------------------------
    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult?) {
            result ?: return
            val dev = result.device ?: return
            val rec = result.scanRecord
            val name = rec?.deviceName ?: dev.name
            val hasUuid =
                rec?.serviceUuids?.any { it.uuid == SERVICE_UUID } == true
            // Keep the device even if the name is missing (only the UUID match).
            if (!hasUuid && (name == null || !name.contains("POCHITA"))) return

            val key = dev.address
            if (!devices.containsKey(key)) {
                devices[key] = dev
                val label = when {
                    name != null -> "$name  [${dev.address}]"
                    hasUuid -> "POCHITA-CHAT?  [${dev.address}]"
                    else -> "[${dev.address}]"
                }
                mainHandler.post { deviceAdapter?.add(label) }
            }
            if (!connected && gatt == null && pendingDevice == null) {
                mainHandler.removeCallbacks(autoConnect)
                mainHandler.postDelayed(autoConnect, 250)
            }
        }

        override fun onScanFailed(errorCode: Int) {
            setStatus("Scan lỗi: $errorCode")
        }
    }

    @SuppressLint("MissingPermission")
    private fun startScan() {
        if (!hasBlePermissions()) {
            requestBlePermissions()
            return
        }
        if (connected) {
            setStatus("Đang kết nối: ${gatt?.device?.name ?: ""}")
            return
        }
        if (scanning) return
        bluetoothAdapter?.takeIf { it.isEnabled }?.let {
            devices.clear()
            deviceAdapter?.clear()
            scanner = it.bluetoothLeScanner
            val settings = ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build()
            try {
                scanner?.startScan(null, settings, scanCallback)
                scanning = true
                setStatus("Đang quét $TARGET_NAME...")
            } catch (e: Exception) {
                Log.e("BLE", "scan", e)
                setStatus("Không bắt đầu được scan: ${e.message}")
            }
        } ?: run {
            setStatus("Bluetooth đang tắt hoặc không có adapter")
        }
    }

    @SuppressLint("MissingPermission")
    private fun stopScan() {
        if (!scanning) return
        scanning = false
        try {
            scanner?.stopScan(scanCallback)
        } catch (_: Exception) {
        }
    }

    // ---- connect ------------------------------------------------------------
    @SuppressLint("MissingPermission")
    private fun connect(dev: BluetoothDevice) {
        if (connected || gatt != null) return
        pendingDevice = dev
        stopScan()
        setStatus("Đang kết nối ${dev.name ?: dev.address}...")
        try {
            gatt = dev.connectGatt(this, false, gattCallback)
        } catch (e: SecurityException) {
            setStatus("Thiếu quyền Bluetooth_CONNECT")
            requestBlePermissions()
        } catch (e: Exception) {
            Log.e("BLE", "connect", e)
            setStatus("Lỗi khi kết nối: ${e.message}")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                try {
                    g.close()
                } catch (_: Exception) {
                }
                gatt = null
                val dev = pendingDevice
                connectAttempts++
                if (connectAttempts >= 3 || dev == null) {
                    connectAttempts = 0
                    pendingDevice = null
                    setStatus("Kết nối thất bại (status=$status). Đang quét lại...")
                    mainHandler.postDelayed({ startScan() }, 1500)
                } else {
                    setStatus("Kết nối lỗi (status=$status). Thử lại...")
                    mainHandler.postDelayed(retryConnect, 1200)
                }
                return
            }
            if (newState == BluetoothGatt.STATE_CONNECTED) {
                connected = true
                connectAttempts = 0
                discoveryAttempts = 0
                gatt = g
                setStatus("Đã kết nối: ${g.device.name ?: g.device.address}")
                runOnUiThread { devicePanel?.visibility = View.GONE }
                try {
                    g.requestMtu(247)
                } catch (_: Exception) {
                }
                g.discoverServices()
            } else if (newState == BluetoothGatt.STATE_DISCONNECTED) {
                connected = false
                rxChar = null
                txChar = null
                try {
                    g.close()
                } catch (_: Exception) {
                }
                gatt = null
                pendingDevice = null
                setStatus("Mất kết nối. Đang quét lại...")
                runOnUiThread { devicePanel?.visibility = View.VISIBLE }
                startScan()
            }
        }

        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
            Log.i("BLE", "MTU=$mtu status=$status")
        }

        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                discoveryAttempts++
                if (discoveryAttempts < 3) {
                    setStatus("Discovery lỗi (status=$status). Thử lại...")
                    mainHandler.postDelayed(retryDiscover, 500)
                } else {
                    discoveryAttempts = 0
                    setStatus("Không đọc được dịch vụ (status=$status). Bấm Quét lại")
                }
                return
            }

            val nus = g.services.firstOrNull {
                it.uuid == SERVICE_UUID || it.uuid.toString().lowercase() == SERVICE_UUID.toString().lowercase()
            }
            if (nus == null) {
                discoveryAttempts++
                if (discoveryAttempts < 3) {
                    setStatus("Dịch vụ chưa đầy đủ (thử $discoveryAttempts/3)...")
                    mainHandler.postDelayed(retryDiscover, 600)
                    return
                }
                discoveryAttempts = 0
                val list = g.services.joinToString(" | ") { it.uuid.toString() }
                try {
                    g.close()
                } catch (_: Exception) {
                }
                gatt = null
                connected = false
                pendingDevice = null
                setStatus("Không có NUS. Dịch vụ tìm thấy: $list")
                runOnUiThread { devicePanel?.visibility = View.VISIBLE }
                return
            }
            discoveryAttempts = 0
            rxChar = nus.getCharacteristic(RX_UUID)
            txChar = nus.getCharacteristic(TX_UUID)
            try {
                txChar?.let { tx ->
                    g.setCharacteristicNotification(tx, true)
                    tx.getDescriptor(CCCD_UUID)?.let { d ->
                        d.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        g.writeDescriptor(d)
                    }
                }
            } catch (e: Exception) {
                Log.e("BLE", "notify setup", e)
            }
            setStatus("Sẵn sàng. Nhập tin nhắn để gửi!")
        }

        override fun onCharacteristicChanged(
            g: BluetoothGatt,
            c: BluetoothGattCharacteristic
        ) {
            if (c.uuid == TX_UUID) {
                c.value?.let { parseIncoming(it) }
            }
        }
    }

    // ---- framing / receive --------------------------------------------------
    private fun parseIncoming(b: ByteArray) {
        val first = b[0].toInt() and 0xff
        if (b.size >= 3 && first == CTRL) {
            when (b[1].toChar()) {
                'M' -> deliver(String(b, 2, b.size - 2, Charsets.UTF_8))
                'S' -> {
                    rxBuf.clear()
                    rxBuf.append(String(b, 2, b.size - 2, Charsets.UTF_8))
                    scheduleFlush()
                }
                'C' -> {
                    rxBuf.append(String(b, 2, b.size - 2, Charsets.UTF_8))
                    scheduleFlush()
                }
                'E' -> {
                    rxBuf.append(String(b, 2, b.size - 2, Charsets.UTF_8))
                    val m = rxBuf.toString()
                    rxBuf.clear()
                    mainHandler.removeCallbacks(frameFlush)
                    deliver(m)
                }
            }
        } else {
            var s = String(b, Charsets.UTF_8)
            s = s.replace("\r", "").replace("\n", " ").trim()
            if (s.isNotEmpty()) deliver(s)
        }
    }

    private fun scheduleFlush() {
        mainHandler.removeCallbacks(frameFlush)
        mainHandler.postDelayed(frameFlush, FRAME_FLUSH_MS)
    }

    private fun deliver(m: String) {
        if (m.isNotEmpty()) appendChat("$DEVICE_TAG: $m")
    }

    // ---- send ---------------------------------------------------------------
    @SuppressLint("MissingPermission")
    private fun send() {
        val g = gatt ?: return
        val rc = rxChar ?: return
        if (!connected) {
            setStatus("Chưa kết nối ESP32")
            return
        }
        val text = inputEt?.text?.toString()?.trim().orEmpty()
        if (text.isEmpty()) return

        val bytes = text.toByteArray(Charsets.UTF_8)
        if (bytes.size <= CHUNK) {
            write(g, rc, bytes)
        } else {
            var pos = 0
            write(g, rc, framed("S", bytes.copyOfRange(0, CHUNK)))
            pos = CHUNK
            while (bytes.size - pos > CHUNK) {
                write(g, rc, framed("C", bytes.copyOfRange(pos, pos + CHUNK)))
                pos += CHUNK
            }
            write(g, rc, framed("E", bytes.copyOfRange(pos, bytes.size)))
        }
        appendChat("Bạn: $text")
        inputEt?.text?.clear()
    }

    private fun framed(type: String, data: ByteArray): ByteArray {
        val out = ByteArray(2 + data.size)
        out[0] = CTRL.toByte()
        out[1] = type.toByteArray()[0]
        System.arraycopy(data, 0, out, 2, data.size)
        return out
    }

    @SuppressLint("MissingPermission")
    private fun write(g: BluetoothGatt, c: BluetoothGattCharacteristic, data: ByteArray) {
        try {
            if (Build.VERSION.SDK_INT >= 33) {
                g.writeCharacteristic(c, data, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
            } else {
                c.value = data
                g.writeCharacteristic(c)
            }
        } catch (e: Exception) {
            Log.e("BLE", "write", e)
        }
    }

    // ---- UI helpers ---------------------------------------------------------
    private fun appendChat(line: String) {
        mainHandler.post {
            chatTv?.append(line + "\n")
            chatScroll?.post { chatScroll?.fullScroll(View.FOCUS_DOWN) }
        }
    }

    private fun setStatus(s: String) {
        mainHandler.post { statusTv?.text = s }
    }

    // ---- permissions --------------------------------------------------------
    private fun requiredPermissions(): Array<String> =
        if (Build.VERSION.SDK_INT >= 31) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }

    private fun hasBlePermissions(): Boolean =
        requiredPermissions().all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }

    private fun requestBlePermissions() {
        permLauncher.launch(requiredPermissions())
    }

    // ---- lifecycle ----------------------------------------------------------
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        statusTv = findViewById(R.id.statusTv)
        chatTv = findViewById(R.id.chatTv)
        chatScroll = findViewById(R.id.chatScroll)
        inputEt = findViewById(R.id.inputEt)
        deviceList = findViewById(R.id.deviceList)
        devicePanel = findViewById(R.id.devicePanel)
        deviceAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, mutableListOf())
        deviceList?.adapter = deviceAdapter

        deviceList?.setOnItemClickListener { _, _, pos, _ ->
            val line = deviceAdapter?.getItem(pos) ?: return@setOnItemClickListener
            val mac = line.substringAfter("[").substringBefore("]")
            devices[mac]?.let { connect(it) }
        }
        findViewById<Button>(R.id.sendBtn)?.setOnClickListener { send() }
        findViewById<Button>(R.id.scanBtn)?.setOnClickListener { startScan() }
        inputEt?.setOnEditorActionListener { _, _, _ -> send(); true }

        bluetoothManager = getSystemService(BluetoothManager::class.java)
        bluetoothAdapter = bluetoothManager?.adapter
        if (bluetoothAdapter == null) {
            setStatus("Thiết bị này không hỗ trợ Bluetooth")
            return
        }

        if (Build.VERSION.SDK_INT < 31) {
            val lm = getSystemService(LOCATION_SERVICE) as LocationManager
            if (!lm.isProviderEnabled(LocationManager.GPS_PROVIDER) &&
                !lm.isProviderEnabled(LocationManager.NETWORK_PROVIDER)
            ) {
                Toast.makeText(this, "Hãy bật Vị trí (Location) để quét BLE", Toast.LENGTH_LONG).show()
            }
        }

        if (hasBlePermissions()) startScan() else requestBlePermissions()
    }

    override fun onDestroy() {
        super.onDestroy()
        mainHandler.removeCallbacksAndMessages(null)
        stopScan()
        try {
            gatt?.close()
        } catch (_: Exception) {
        }
    }
}

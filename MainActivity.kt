package com.example.mayaptrung

import androidx.compose.material3.Typography
import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Image
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Divider
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import androidx.compose.ui.unit.sp
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.SocketTimeoutException
import java.io.IOException
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.UUID

// -----------------------------------------------------------------------------
// FONT VÀ MATERIAL 3 THEME
// Đặt 3 file font trong app/src/main/res/font/ theo hướng dẫn ở cuối câu trả lời.
// -----------------------------------------------------------------------------
private val SfProDisplay = FontFamily(
    Font(R.font.sf_pro_regular, FontWeight.Normal),
    Font(R.font.sf_pro_medium, FontWeight.Medium),
    Font(R.font.sf_pro_bold, FontWeight.Bold)
)

private val AppTypography = Typography(
    displayLarge = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Bold, fontSize = 57.sp, lineHeight = 64.sp),
    displayMedium = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Bold, fontSize = 45.sp, lineHeight = 52.sp),
    displaySmall = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Medium, fontSize = 36.sp, lineHeight = 44.sp),
    headlineLarge = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Bold, fontSize = 32.sp, lineHeight = 40.sp),
    headlineMedium = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Bold, fontSize = 28.sp, lineHeight = 36.sp),
    headlineSmall = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Medium, fontSize = 24.sp, lineHeight = 32.sp),
    titleLarge = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Bold, fontSize = 22.sp, lineHeight = 28.sp),
    titleMedium = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Medium, fontSize = 16.sp, lineHeight = 24.sp),
    titleSmall = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Medium, fontSize = 14.sp, lineHeight = 20.sp),
    bodyLarge = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Normal, fontSize = 16.sp, lineHeight = 24.sp),
    bodyMedium = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Normal, fontSize = 14.sp, lineHeight = 20.sp),
    bodySmall = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Normal, fontSize = 12.sp, lineHeight = 16.sp),
    labelLarge = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Medium, fontSize = 14.sp, lineHeight = 20.sp),
    labelMedium = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Medium, fontSize = 12.sp, lineHeight = 16.sp),
    labelSmall = TextStyle(fontFamily = SfProDisplay, fontWeight = FontWeight.Normal, fontSize = 11.sp, lineHeight = 16.sp)
)


private val AppColors = darkColorScheme(
    primary = Color(0xFF0A7CFF),
    onPrimary = Color.White,
    primaryContainer = Color(0xFF123B68),
    onPrimaryContainer = Color(0xFFD7E8FF),
    secondary = Color(0xFF67B4FF),
    onSecondary = Color(0xFF002B50),
    secondaryContainer = Color(0xFF173B5E),
    onSecondaryContainer = Color(0xFFD5E9FF),
    background = Color(0xFF000000),
    onBackground = Color(0xFFFFFFFF),
    surface = Color(0xFF1E1E1E),
    onSurface = Color(0xFFE4E6EB),
    surfaceVariant = Color(0xFF242526),
    onSurfaceVariant = Color(0xFFB0B3B8),
    outline = Color(0xFF3A3B3C),
    outlineVariant = Color(0xFF2B2C2D),
    error = Color(0xFFFF6B6B),
    onError = Color(0xFF2B0000),
    errorContainer = Color(0xFF5A1D1D),
    onErrorContainer = Color(0xFFFFDAD6)
)

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme(colorScheme = AppColors, typography = AppTypography) {
                Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
                    MayApTrungApp()
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// API CLIENT — giữ nguyên endpoint, HTTP method, header và payload của mã gốc.
// -----------------------------------------------------------------------------
private object ApiClient {
    /**
     * Đánh thức Render trước khi đăng nhập.
     * GET / chỉ dùng để kiểm tra web service đã phản hồi; không thay endpoint API.
     * HTTP 4xx (ví dụ 404 ở trang gốc) vẫn chứng minh service đã thức.
     */
    suspend fun waitForBackend(baseUrl: String) = withContext(Dispatchers.IO) {
        val normalized = baseUrl.trim().trimEnd('/')
        require(normalized.startsWith("http://") || normalized.startsWith("https://")) {
            "Địa chỉ API phải bắt đầu bằng http:// hoặc https://"
        }

        val deadline = System.nanoTime() + 90_000_000_000L // tối đa 90 giây
        var lastProblem = "Chưa nhận được phản hồi"

        while (System.nanoTime() < deadline) {
            var connection: HttpURLConnection? = null
            try {
                connection = (URL("$normalized/").openConnection() as HttpURLConnection).apply {
                    requestMethod = "GET"
                    connectTimeout = 8_000
                    readTimeout = 8_000
                    setRequestProperty("Accept", "*/*")
                    setRequestProperty("Connection", "close")
                }

                val code = connection.responseCode
                // 2xx–4xx: ứng dụng/backend đã phản hồi. 5xx: chờ Render thử khởi động lại.
                if (code in 200..499) return@withContext
                lastProblem = "Máy chủ tạm trả HTTP $code"
            } catch (e: Exception) {
                lastProblem = e.message ?: e.javaClass.simpleName
            } finally {
                try { connection?.errorStream?.close() } catch (_: Exception) { }
                try { connection?.inputStream?.close() } catch (_: Exception) { }
                connection?.disconnect()
            }
            delay(2_500)
        }

        throw IOException(
            "Không đánh thức được máy chủ sau 90 giây. $lastProblem. " +
                    "Kiểm tra Internet hoặc trạng thái dịch vụ Render rồi thử lại."
        )
    }

    suspend fun request(
        baseUrl: String,
        path: String,
        method: String = "GET",
        token: String? = null,
        body: JSONObject? = null,
        connectTimeoutMs: Int = 10_000,
        readTimeoutMs: Int = 10_000
    ): JSONObject = withContext(Dispatchers.IO) {
        val normalized = baseUrl.trim().trimEnd('/')
        require(normalized.startsWith("http://") || normalized.startsWith("https://")) {
            "Địa chỉ API phải bắt đầu bằng http:// hoặc https://"
        }
        val connection = (URL(normalized + path).openConnection() as HttpURLConnection).apply {
            requestMethod = method
            // Login có thể đánh thức Render Free đang ngủ; thời gian cold start có thể khoảng 1 phút.
            connectTimeout = connectTimeoutMs
            readTimeout = readTimeoutMs
            setRequestProperty("Accept", "application/json")
            if (!token.isNullOrBlank()) setRequestProperty("Authorization", "Bearer $token")
            if (body != null) {
                doOutput = true
                setRequestProperty("Content-Type", "application/json; charset=utf-8")
            }
        }
        try {
            if (body != null) connection.outputStream.use {
                it.write(body.toString().toByteArray(Charsets.UTF_8))
            }
            val code = connection.responseCode
            val stream = if (code in 200..299) connection.inputStream else connection.errorStream
            val response = stream?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }.orEmpty()
            val json = try { JSONObject(response) } catch (_: Exception) { JSONObject() }
            if (code !in 200..299) {
                throw IllegalStateException(
                    json.optString("error").ifBlank { "Máy chủ trả về HTTP $code" }
                )
            }
            json
        } finally {
            connection.disconnect()
        }
    }
}

// -----------------------------------------------------------------------------
// BLUETOOTH SPP — giữ nguyên cách ghép đôi, UUID, JSON login và wifi_config.
// -----------------------------------------------------------------------------
private suspend fun sendWifiByBluetooth(
    context: Context,
    setupPassword: String,
    ssid: String,
    wifiPassword: String
) = withContext(Dispatchers.IO) {
    require(ssid.isNotBlank() && ssid.length <= 32) {
        "SSID Wi-Fi không hợp lệ (1–32 ký tự)."
    }
    require(wifiPassword.length <= 63) { "Mật khẩu Wi-Fi quá dài." }
    val adapter = BluetoothAdapter.getDefaultAdapter() ?: error("Điện thoại không có Bluetooth.")
    if (!adapter.isEnabled) error("Hãy bật Bluetooth trên điện thoại.")
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
        context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED
    ) error("Chưa được cấp quyền Bluetooth Connect.")
    val device = adapter.bondedDevices.firstOrNull { it.name == "HT-MayApTrung" }
        ?: error("Chưa ghép đôi HT-MayApTrung. Hãy ghép đôi trong Cài đặt Bluetooth trước.")
    val sppUuid = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
    var socket: BluetoothSocket? = null
    try {
        socket = device.createRfcommSocketToServiceRecord(sppUuid)
        socket.connect()
        val login = JSONObject().put(
            "login",
            JSONObject().put("username", "device").put("password", setupPassword)
        )
        socket.outputStream.write((login.toString() + "\n").toByteArray(Charsets.UTF_8))
        socket.outputStream.flush()
        Thread.sleep(300)
        val config = JSONObject().put(
            "wifi_config",
            JSONObject().put("ssid", ssid).put("password", wifiPassword)
        )
        socket.outputStream.write((config.toString() + "\n").toByteArray(Charsets.UTF_8))
        socket.outputStream.flush()
        Thread.sleep(250)
    } finally {
        try { socket?.close() } catch (_: Exception) { }
    }
}

private val logCategories = listOf(
    "Tất cả nhật ký", "Nguồn", "Cảm biến", "Đèn sưởi", "Quạt",
    "Cảnh báo", "Mạng", "Điều khiển", "Bluetooth"
)

private enum class MainTab(val title: String, val route: String) {
    HOME("Trang chủ", "home"),
    LOGS("Nhật ký", "logs"),
    SETTINGS("Cài đặt", "settings")
}

private fun tabIndex(route: String?): Int = when (route) {
    MainTab.HOME.route -> 0
    MainTab.LOGS.route -> 1
    MainTab.SETTINGS.route -> 2
    else -> 0
}

// State trình bày riêng cho UI. Chỉ cập nhật từ response /api/state của phiên hiện tại.
private data class DeviceUiState(
    val temperature: String = "--",
    val humidity: String = "--",
    val mq2Raw: String = "--",
    val heater: String = "TẮT",
    val fan: String = "TẮT",
    val buzzer: String = "TẮT",
    val power: String = "--",
    val sensorStatus: String = "Lỗi kết nối",
    val gasStatus: String = "--",
    val overTempStatus: String = "--",
    val heaterSource: String = "--",
    val fanSource: String = "--"
) {
    companion object {
        fun fromJson(json: JSONObject): DeviceUiState {
            val sensorOk = json.optBoolean("sensor_ok", false) &&
                    !json.isNull("temp") && !json.isNull("hum")
            val hasMq2 = json.has("mq2_raw") && !json.isNull("mq2_raw")
            return DeviceUiState(
                temperature = if (sensorOk) String.format(
                    Locale.US, "%.1f °C", json.optDouble("temp")
                ) else "--",
                humidity = if (sensorOk) "${json.optDouble("hum").toInt()} %" else "--",
                mq2Raw = if (hasMq2) json.optInt("mq2_raw").toString() else "--",
                heater = if (json.optBoolean("heater_on", false)) "BẬT" else "TẮT",
                fan = if (json.optBoolean("fan_on", false)) "BẬT" else "TẮT",
                buzzer = if (json.optBoolean("buzzer_on", false)) "ĐANG KÊU"
                else if (json.optBoolean("buzzer_enabled", true)) "Cho phép"
                else "Tắt tiếng",
                power = if (json.has("power")) {
                    if (json.optBoolean("power")) "BẬT" else "TẮT"
                } else "--",
                sensorStatus = if (sensorOk) "Đang hoạt động" else "Lỗi cảm biến",
                gasStatus = if (json.optBoolean("gas_alarm")) "CẢNH BÁO" else "Bình thường",
                overTempStatus = if (json.optBoolean("overtemp")) "ĐANG KHÓA" else "Bình thường",
                heaterSource = json.optString("heater_source", "--"),
                fanSource = json.optString("fan_source", "--")
            )
        }
    }
}

@Composable
private fun MayApTrungApp() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    // URL Render theo địa chỉ đang xuất hiện trong ứng dụng bạn gửi.
    var baseUrl by remember { mutableStateOf("https://mayaptrung-backend.onrender.com") }
    var username by remember { mutableStateOf("admin") }
    var password by remember { mutableStateOf("") }
    var token by remember { mutableStateOf<String?>(null) }

    var error by remember { mutableStateOf("") }
    var notice by remember { mutableStateOf("") }
    var isLoggingIn by remember { mutableStateOf(false) }
    var isSendingWifi by remember { mutableStateOf(false) }

    // Đánh thức Render ngay khi mở app, trong lúc người dùng nhập thông tin.
    var isBackendWarming by remember { mutableStateOf(true) }
    var backendStatusText by remember {
        mutableStateOf("Đang khởi động máy chủ; lần đầu có thể mất khoảng một phút...")
    }

    // State realtime không đọc từ cache. Mở app hoặc đăng xuất đều bắt đầu bằng dữ liệu trống.
    var deviceState by remember { mutableStateOf(DeviceUiState()) }
    var isConnected by remember { mutableStateOf(false) }

    // Danh sách log được giữ lại khi đăng xuất, đúng yêu cầu không xóa lịch sử trong RAM.
    var logs by remember { mutableStateOf(listOf<JSONObject>()) }
    var selectedCategory by remember { mutableStateOf(-1) }
    var categoryMenuOpen by remember { mutableStateOf(false) }
    var selectedTab by remember { mutableStateOf(MainTab.HOME) }

    var wifiSsid by remember { mutableStateOf("") }
    var wifiPassword by remember { mutableStateOf("") }
    var bluetoothSetupPassword by remember { mutableStateOf("") }

    var showDeleteGroupDialog by remember { mutableStateOf(false) }
    var logPendingDelete by remember { mutableStateOf<JSONObject?>(null) }

    // Xóa hoàn toàn dữ liệu tức thời; không đụng tới danh sách logs.
    fun resetDeviceState() {
        deviceState = DeviceUiState()
        isConnected = false
    }

    // Khi khởi tạo app, tuyệt đối không hiển thị dữ liệu cũ của phiên trước.
    LaunchedEffect(Unit) {
        resetDeviceState()
    }

    // Thông báo đăng nhập chỉ xuất hiện ngắn ở trang Chủ, không lưu lại trên các tab khác.
    LaunchedEffect(notice) {
        if (notice.startsWith("Đăng nhập thành công.")) {
            delay(3_000)
            if (notice.startsWith("Đăng nhập thành công.")) notice = ""
        }
    }

    // Bắt đầu đánh thức backend ngay khi app mở; người dùng có thể nhập tài khoản trong lúc chờ.
    LaunchedEffect(baseUrl) {
        isBackendWarming = true
        backendStatusText = "Đang đánh thức máy chủ Render..."
        try {
            ApiClient.waitForBackend(baseUrl)
            backendStatusText = "Máy chủ đã sẵn sàng. Bạn có thể đăng nhập."
        } catch (e: Exception) {
            backendStatusText = "Chưa kết nối được máy chủ. Nhấn Đăng nhập để thử kết nối lại."
        } finally {
            isBackendWarming = false
        }
    }

    suspend fun reload() {
        val currentToken = token ?: return

        // Lấy trạng thái mới từ backend. Chỉ hiển thị dữ liệu khi server báo online.
        val status = ApiClient.request(baseUrl, "/api/state", token = currentToken)
        if (token != currentToken) return

        val device = status.optJSONObject("state") ?: JSONObject()
        val online = status.optBoolean("online", false) && device.length() > 0
        isConnected = online
        deviceState = if (online) DeviceUiState.fromJson(device) else DeviceUiState()

        // Nhật ký lấy từ đúng endpoint cũ; lỗi log không làm mất state cảm biến vừa tải.
        try {
            val logResponse = ApiClient.request(
                baseUrl,
                "/api/logs?category=$selectedCategory&limit=150",
                token = currentToken
            )
            if (token != currentToken) return
            val array = logResponse.optJSONArray("logs") ?: JSONArray()
            logs = (0 until array.length()).mapNotNull { index -> array.optJSONObject(index) }
        } catch (e: Exception) {
            error = e.message ?: "Không tải được nhật ký."
        }
    }

    LaunchedEffect(token, baseUrl, selectedCategory) {
        val activeToken = token ?: return@LaunchedEffect
        while (token == activeToken) {
            try {
                reload()
                if (token == activeToken) error = ""
            } catch (e: Exception) {
                if (token == activeToken) {
                    resetDeviceState()
                    error = e.message ?: "Không tải được dữ liệu."
                }
            }
            delay(2500)
        }
    }

    suspend fun sendWifiConfiguration() {
        isSendingWifi = true
        error = ""
        try {
            sendWifiByBluetooth(context, bluetoothSetupPassword, wifiSsid, wifiPassword)
            notice = "Đã gửi cấu hình Wi-Fi qua Bluetooth. Hãy chờ ESP32 kết nối router."
        } catch (e: Exception) {
            error = e.message ?: "Không cấu hình được Wi-Fi qua Bluetooth."
        } finally {
            isSendingWifi = false
        }
    }

    val bluetoothPermissionLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestPermission()
    ) { granted ->
        if (granted) {
            scope.launch { sendWifiConfiguration() }
        } else {
            error = "Bạn cần cấp quyền Bluetooth Connect để cấu hình Wi-Fi."
        }
    }

    if (token == null) {
        LoginScreen(
            username = username,
            onUsernameChange = { username = it },
            password = password,
            onPasswordChange = { password = it },
            error = error,
            statusMessage = backendStatusText,
            isBusy = isLoggingIn || isBackendWarming,
            isWarming = isBackendWarming,
            onLogin = {
                scope.launch {
                    isLoggingIn = true
                    error = ""
                    notice = ""
                    try {
                        // Kiểm tra service đã thức trước; tránh POST login trong lúc Render còn khởi động.
                        isBackendWarming = true
                        backendStatusText = "Đang đánh thức / xác nhận máy chủ Render..."
                        ApiClient.waitForBackend(baseUrl)
                        isBackendWarming = false
                        backendStatusText = "Máy chủ đã sẵn sàng. Đang xác thực tài khoản..."

                        val result = ApiClient.request(
                            baseUrl,
                            "/api/login",
                            "POST",
                            body = JSONObject()
                                .put("username", username.trim())
                                .put("password", password),
                            connectTimeoutMs = 15_000,
                            readTimeoutMs = 30_000
                        )
                        val newToken = result.getString("token")
                        resetDeviceState()
                        token = newToken
                        password = ""
                        selectedTab = MainTab.HOME
                        notice = "Đăng nhập thành công. Đang tải dữ liệu mới từ máy chủ..."
                    } catch (e: SocketTimeoutException) {
                        error = "Máy chủ đã được đánh thức nhưng API đăng nhập không phản hồi trong 30 giây. " +
                                "Kiểm tra log backend Render. Lỗi này chưa kết luận tài khoản hoặc mật khẩu sai."
                    } catch (e: Exception) {
                        error = e.message ?: "Đăng nhập thất bại."
                    } finally {
                        isBackendWarming = false
                        isLoggingIn = false
                    }
                }
            }
        )
    } else {
        MainShell(
            selectedTab = selectedTab,
            onTabSelected = { selectedTab = it },
            isConnected = isConnected,
            deviceState = deviceState,
            logs = logs,
            selectedCategory = selectedCategory,
            categoryMenuOpen = categoryMenuOpen,
            onCategoryMenuOpenChange = { categoryMenuOpen = it },
            onCategorySelected = { selectedCategory = it },
            notice = notice,
            error = error,
            onRefreshLogs = {
                scope.launch {
                    try {
                        reload()
                        notice = "Đã làm mới dữ liệu."
                    } catch (e: Exception) {
                        error = e.message ?: "Không làm mới được dữ liệu."
                    }
                }
            },
            onDeleteGroup = { showDeleteGroupDialog = true },
            onDeleteLog = { logPendingDelete = it },
            onCommand = { command ->
                runCommand(baseUrl, token, command, scope) { message, failed ->
                    if (failed) error = message else notice = message
                }
            },
            wifiSsid = wifiSsid,
            onWifiSsidChange = { wifiSsid = it },
            wifiPassword = wifiPassword,
            onWifiPasswordChange = { wifiPassword = it },
            bluetoothSetupPassword = bluetoothSetupPassword,
            onBluetoothPasswordChange = { bluetoothSetupPassword = it },
            isSendingWifi = isSendingWifi,
            onSendWifi = {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S &&
                    context.checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED
                ) {
                    bluetoothPermissionLauncher.launch(Manifest.permission.BLUETOOTH_CONNECT)
                } else {
                    scope.launch { sendWifiConfiguration() }
                }
            },
            onLogout = {
                // Không xóa logs; chỉ xóa token và dữ liệu realtime đang hiển thị.
                token = null
                password = ""
                resetDeviceState()
                selectedTab = MainTab.HOME
                error = ""
                notice = "Đã đăng xuất. Nhật ký hiện có vẫn được giữ trong phiên ứng dụng."
            }
        )
    }

    if (showDeleteGroupDialog) {
        AlertDialog(
            onDismissRequest = { showDeleteGroupDialog = false },
            title = { Text("Xóa nhóm nhật ký?") },
            text = { Text("Thao tác này sẽ gọi API xóa nhóm nhật ký đang chọn.") },
            confirmButton = {
                TextButton(onClick = {
                    showDeleteGroupDialog = false
                    scope.launch {
                        try {
                            val cleared = ApiClient.request(
                                baseUrl,
                                "/api/logs?category=$selectedCategory",
                                "DELETE",
                                token
                            )
                            notice = if (cleared.optBoolean("local_clear_command_published")) {
                                "Đã xóa cloud và gửi lệnh xóa nhật ký cục bộ đến ESP32."
                            } else {
                                "Đã xóa nhật ký cloud; ESP32 offline nên nhật ký cục bộ chưa được xóa."
                            }
                            reload()
                        } catch (e: Exception) {
                            error = e.message ?: "Không xóa được nhật ký."
                        }
                    }
                }) { Text("Xóa nhóm") }
            },
            dismissButton = {
                TextButton(onClick = { showDeleteGroupDialog = false }) { Text("Hủy") }
            }
        )
    }

    logPendingDelete?.let { log ->
        AlertDialog(
            onDismissRequest = { logPendingDelete = null },
            title = { Text("Xóa nhật ký này?") },
            text = { Text(log.optString("message")) },
            confirmButton = {
                TextButton(onClick = {
                    logPendingDelete = null
                    scope.launch {
                        try {
                            ApiClient.request(
                                baseUrl,
                                "/api/logs/${log.optLong("id")}",
                                "DELETE",
                                token
                            )
                            reload()
                        } catch (e: Exception) {
                            error = e.message ?: "Không xóa được nhật ký."
                        }
                    }
                }) { Text("Xóa") }
            },
            dismissButton = {
                TextButton(onClick = { logPendingDelete = null }) { Text("Hủy") }
            }
        )
    }
}

// -----------------------------------------------------------------------------
// LOGIN — logo trung tâm, hai trường nhập liệu và nút đăng nhập.
// -----------------------------------------------------------------------------
@Composable
private fun LoginScreen(
    username: String,
    onUsernameChange: (String) -> Unit,
    password: String,
    onPasswordChange: (String) -> Unit,
    error: String,
    statusMessage: String,
    isBusy: Boolean,
    isWarming: Boolean,
    onLogin: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 28.dp, vertical = 24.dp),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Image(
            painter = painterResource(id = R.drawable.ht_mat_logo),
            contentDescription = "Logo HT-MAT",
            modifier = Modifier.size(132.dp),
            contentScale = ContentScale.Fit
        )
        Spacer(Modifier.height(42.dp))
        OutlinedTextField(
            value = username,
            onValueChange = onUsernameChange,
            modifier = Modifier.fillMaxWidth(),
            label = { Text("Tài khoản") },
            singleLine = true,
            shape = RoundedCornerShape(18.dp),
            colors = OutlinedTextFieldDefaults.colors(
                focusedBorderColor = MaterialTheme.colorScheme.primary,
                focusedLabelColor = MaterialTheme.colorScheme.primary,
                cursorColor = MaterialTheme.colorScheme.primary
            )
        )
        Spacer(Modifier.height(16.dp))
        OutlinedTextField(
            value = password,
            onValueChange = onPasswordChange,
            modifier = Modifier.fillMaxWidth(),
            label = { Text("Mật khẩu") },
            singleLine = true,
            visualTransformation = PasswordVisualTransformation(),
            shape = RoundedCornerShape(18.dp),
            colors = OutlinedTextFieldDefaults.colors(
                focusedBorderColor = MaterialTheme.colorScheme.primary,
                focusedLabelColor = MaterialTheme.colorScheme.primary,
                cursorColor = MaterialTheme.colorScheme.primary
            )
        )
        Spacer(Modifier.height(22.dp))
        Button(
            onClick = onLogin,
            enabled = !isBusy && username.isNotBlank() && password.isNotBlank(),
            modifier = Modifier.fillMaxWidth().height(56.dp),
            shape = RoundedCornerShape(30.dp),
            colors = ButtonDefaults.buttonColors(
                containerColor = MaterialTheme.colorScheme.primary,
                contentColor = MaterialTheme.colorScheme.onPrimary
            )
        ) {
            if (isBusy) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    CircularProgressIndicator(
                        modifier = Modifier.size(18.dp),
                        strokeWidth = 2.dp,
                        color = MaterialTheme.colorScheme.onPrimary
                    )
                    Spacer(Modifier.width(10.dp))
                    Text(if (isWarming) "ĐANG KẾT NỐI MÁY CHỦ..." else "ĐANG ĐĂNG NHẬP...")
                }
            } else {
                Text("Đăng nhập", style = MaterialTheme.typography.titleMedium)
            }
        }
        if (statusMessage.isNotBlank()) {
            Text(
                text = statusMessage,
                modifier = Modifier.fillMaxWidth().padding(top = 12.dp),
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                style = MaterialTheme.typography.bodySmall
            )
        }
        if (error.isNotBlank()) {
            Text(
                text = error,
                modifier = Modifier.fillMaxWidth().padding(top = 10.dp),
                color = MaterialTheme.colorScheme.error,
                style = MaterialTheme.typography.bodyMedium
            )
        }
    }
}

// -----------------------------------------------------------------------------
// MAIN SHELL — NavHost cách ly từng trang + thanh điều hướng có pill trượt.
// -----------------------------------------------------------------------------
@Composable
private fun MainShell(
    selectedTab: MainTab,
    onTabSelected: (MainTab) -> Unit,
    isConnected: Boolean,
    deviceState: DeviceUiState,
    logs: List<JSONObject>,
    selectedCategory: Int,
    categoryMenuOpen: Boolean,
    onCategoryMenuOpenChange: (Boolean) -> Unit,
    onCategorySelected: (Int) -> Unit,
    notice: String,
    error: String,
    onRefreshLogs: () -> Unit,
    onDeleteGroup: () -> Unit,
    onDeleteLog: (JSONObject) -> Unit,
    onCommand: (JSONObject) -> Unit,
    wifiSsid: String,
    onWifiSsidChange: (String) -> Unit,
    wifiPassword: String,
    onWifiPasswordChange: (String) -> Unit,
    bluetoothSetupPassword: String,
    onBluetoothPasswordChange: (String) -> Unit,
    isSendingWifi: Boolean,
    onSendWifi: () -> Unit,
    onLogout: () -> Unit
) {
    val navController = rememberNavController()
    val backStackEntry by navController.currentBackStackEntryAsState()
    val currentRoute = backStackEntry?.destination?.route
    val activeTab = MainTab.values().firstOrNull { it.route == currentRoute } ?: selectedTab
    val noticeForSecondaryTabs = if (notice.startsWith("Đăng nhập thành công.")) "" else notice

    // Đồng bộ nút đang chọn khi người dùng sử dụng nút Back của Android.
    LaunchedEffect(currentRoute) {
        if (activeTab != selectedTab) onTabSelected(activeTab)
    }

    Scaffold(
        modifier = Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background),
        bottomBar = {
            MessengerBottomBar(
                selectedTab = activeTab,
                onSelect = { tab ->
                    if (tab != activeTab) {
                        onTabSelected(tab)
                        navController.navigate(tab.route) {
                            popUpTo(MainTab.HOME.route) { saveState = true }
                            launchSingleTop = true
                            restoreState = true
                        }
                    }
                }
            )
        },
        containerColor = MaterialTheme.colorScheme.background
    ) { innerPadding ->
        NavHost(
            navController = navController,
            startDestination = MainTab.HOME.route,
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
                .background(MaterialTheme.colorScheme.background),
            enterTransition = {
                val from = tabIndex(initialState.destination.route)
                val to = tabIndex(targetState.destination.route)
                slideInHorizontally(
                    animationSpec = tween(280),
                    initialOffsetX = { width -> if (to >= from) width else -width }
                ) + fadeIn(animationSpec = tween(180))
            },
            exitTransition = {
                val from = tabIndex(initialState.destination.route)
                val to = tabIndex(targetState.destination.route)
                slideOutHorizontally(
                    animationSpec = tween(280),
                    targetOffsetX = { width -> if (to >= from) -width else width }
                ) + fadeOut(animationSpec = tween(180))
            },
            popEnterTransition = {
                val from = tabIndex(initialState.destination.route)
                val to = tabIndex(targetState.destination.route)
                slideInHorizontally(
                    animationSpec = tween(280),
                    initialOffsetX = { width -> if (to >= from) width else -width }
                ) + fadeIn(animationSpec = tween(180))
            },
            popExitTransition = {
                val from = tabIndex(initialState.destination.route)
                val to = tabIndex(targetState.destination.route)
                slideOutHorizontally(
                    animationSpec = tween(280),
                    targetOffsetX = { width -> if (to >= from) -width else width }
                ) + fadeOut(animationSpec = tween(180))
            }
        ) {
            composable(MainTab.HOME.route) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    HomeTab(
                        device = deviceState,
                        isConnected = isConnected,
                        notice = notice,
                        error = error,
                        onCommand = onCommand
                    )
                }
            }
            composable(MainTab.LOGS.route) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    LogsTab(
                        logs = logs,
                        selectedCategory = selectedCategory,
                        categoryMenuOpen = categoryMenuOpen,
                        onCategoryMenuOpenChange = onCategoryMenuOpenChange,
                        onCategorySelected = onCategorySelected,
                        onRefresh = onRefreshLogs,
                        onDeleteGroup = onDeleteGroup,
                        onDeleteLog = onDeleteLog,
                        notice = noticeForSecondaryTabs,
                        error = error
                    )
                }
            }
            composable(MainTab.SETTINGS.route) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    SettingsTab(
                        wifiSsid = wifiSsid,
                        onWifiSsidChange = onWifiSsidChange,
                        wifiPassword = wifiPassword,
                        onWifiPasswordChange = onWifiPasswordChange,
                        bluetoothSetupPassword = bluetoothSetupPassword,
                        onBluetoothPasswordChange = onBluetoothPasswordChange,
                        isSendingWifi = isSendingWifi,
                        onSendWifi = onSendWifi,
                        onLogout = onLogout,
                        notice = noticeForSecondaryTabs,
                        error = error
                    )
                }
            }
        }
    }
}

// Bottom Navigation Bar theo mẫu PDF: viên thuốc than, ba biểu tượng không nhãn,
// vùng chọn tối hơn và biểu tượng được chọn màu xanh dương.
@Composable
private fun MessengerBottomBar(
    selectedTab: MainTab,
    onSelect: (MainTab) -> Unit
) {
    val tabs = MainTab.values().toList()

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .background(Color.Black)
            .navigationBarsPadding()
            .padding(horizontal = 24.dp, vertical = 9.dp),
        contentAlignment = Alignment.Center
    ) {
        BoxWithConstraints(
            modifier = Modifier
                .fillMaxWidth()
                .height(72.dp)
                .clip(RoundedCornerShape(40.dp))
                .background(Color(0xFF222222))
        ) {
            val itemWidth = maxWidth / tabs.size
            val selectedIndex = tabs.indexOf(selectedTab).coerceAtLeast(0)
            val highlightWidth = 84.dp
            val targetOffset = itemWidth * selectedIndex + (itemWidth - highlightWidth) / 2
            val highlightOffset by androidx.compose.animation.core.animateDpAsState(
                targetValue = targetOffset,
                animationSpec = tween(280),
                label = "bottomNavigationHighlight"
            )

            Box(
                modifier = Modifier
                    .offset(x = highlightOffset)
                    .padding(vertical = 8.dp)
                    .size(width = highlightWidth, height = 56.dp)
                    .clip(RoundedCornerShape(30.dp))
                    .background(Color(0xFF1B1B1B))
            )

            Row(modifier = Modifier.fillMaxSize()) {
                tabs.forEach { tab ->
                    val selected = tab == selectedTab
                    Box(
                        modifier = Modifier
                            .weight(1f)
                            .fillMaxHeight()
                            .semantics { contentDescription = tab.title }
                            .clickable { onSelect(tab) },
                        contentAlignment = Alignment.Center
                    ) {
                        BottomNavigationSymbol(
                            tab = tab,
                            tint = if (selected) Color(0xFF087CFF) else Color(0xFFB8B8B8),
                            modifier = Modifier.size(36.dp)
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun BottomNavigationSymbol(
    tab: MainTab,
    tint: Color,
    modifier: Modifier = Modifier
) {
    Canvas(modifier = modifier) {
        val unit = size.minDimension / 32f
        fun point(x: Float, y: Float) = androidx.compose.ui.geometry.Offset(x * unit, y * unit)
        val outline = Stroke(
            width = 2.2.dp.toPx(),
            cap = StrokeCap.Round,
            join = StrokeJoin.Round
        )

        when (tab) {
            MainTab.HOME -> {
                val house = Path().apply {
                    moveTo(4f * unit, 14f * unit)
                    lineTo(16f * unit, 4f * unit)
                    lineTo(28f * unit, 14f * unit)
                    lineTo(28f * unit, 27f * unit)
                    quadraticTo(28f * unit, 29f * unit, 26f * unit, 29f * unit)
                    lineTo(6f * unit, 29f * unit)
                    quadraticTo(4f * unit, 29f * unit, 4f * unit, 27f * unit)
                    close()
                }
                drawPath(house, tint, style = outline)

                val doorway = Path().apply {
                    moveTo(12.5f * unit, 29f * unit)
                    lineTo(12.5f * unit, 21f * unit)
                    quadraticTo(12.5f * unit, 18.5f * unit, 15f * unit, 18.5f * unit)
                    lineTo(17f * unit, 18.5f * unit)
                    quadraticTo(19.5f * unit, 18.5f * unit, 19.5f * unit, 21f * unit)
                    lineTo(19.5f * unit, 29f * unit)
                }
                drawPath(doorway, tint, style = outline)
            }

            MainTab.LOGS -> {
                val bell = Path().apply {
                    moveTo(16f * unit, 4f * unit)
                    cubicTo(12f * unit, 4f * unit, 10f * unit, 7f * unit, 10f * unit, 12f * unit)
                    lineTo(9f * unit, 21f * unit)
                    cubicTo(9f * unit, 23.5f * unit, 8f * unit, 25f * unit, 6f * unit, 26f * unit)
                    lineTo(26f * unit, 26f * unit)
                    cubicTo(24f * unit, 25f * unit, 23f * unit, 23.5f * unit, 23f * unit, 21f * unit)
                    lineTo(22f * unit, 12f * unit)
                    cubicTo(22f * unit, 7f * unit, 20f * unit, 4f * unit, 16f * unit, 4f * unit)
                    close()
                }
                drawPath(bell, tint)
                drawCircle(tint, radius = 2.3.dp.toPx(), center = point(16f, 29f))
            }

            MainTab.SETTINGS -> {
                val barStroke = 3.1.dp.toPx()
                listOf(9f, 16f, 23f).forEach { y ->
                    drawLine(
                        color = tint,
                        start = point(5f, y),
                        end = point(27f, y),
                        strokeWidth = barStroke,
                        cap = StrokeCap.Round
                    )
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// TAB 1 — thẻ dữ liệu tối giản, nhóm điều khiển không chứa điều khiển MQ2.
// -----------------------------------------------------------------------------
@Composable
private fun HomeTab(
    device: DeviceUiState,
    isConnected: Boolean,
    notice: String,
    error: String,
    onCommand: (JSONObject) -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
            .verticalScroll(rememberScrollState())
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        Text(
            text = "HỆ THỐNG GIÁM SÁT MÁY ẤP TRỨNG",
            modifier = Modifier.fillMaxWidth(),
            fontFamily = SfProDisplay,
            fontSize = 22.sp,
            lineHeight = 29.sp,
            fontWeight = FontWeight.Bold,
            color = MaterialTheme.colorScheme.onBackground,
            maxLines = 2,
            overflow = TextOverflow.Clip
        )
        FeedbackCard(notice, error)
        Text("Tổng quan thiết bị", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)

        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            MetricCard("Nhiệt độ", device.temperature, Modifier.weight(1f))
            MetricCard("Độ ẩm", device.humidity, Modifier.weight(1f))
        }
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            MetricCard("MQ2 ADC", device.mq2Raw, Modifier.weight(1f))
            MetricCard("Đèn sưởi", device.heater, Modifier.weight(1f))
        }
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            MetricCard("Quạt", device.fan, Modifier.weight(1f))
            MetricCard("Còi", device.buzzer, Modifier.weight(1f))
        }

        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(22.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
        ) {
            Column(
                modifier = Modifier.padding(18.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                Text("Trạng thái hệ thống", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                StatusLine("Kết nối ESP32", if (isConnected) "Đang hoạt động" else "Chưa kết nối", isConnected)
                StatusLine("Nguồn hệ thống", device.power, device.power == "BẬT")
                StatusLine("Cảm biến", device.sensorStatus, device.sensorStatus == "Đang hoạt động")
                StatusLine("Khí gas MQ2", device.gasStatus, device.gasStatus == "Bình thường")
                StatusLine("Liên động quá nhiệt", device.overTempStatus, device.overTempStatus == "Bình thường")
                Divider(color = MaterialTheme.colorScheme.outline)
                Text(
                    "Nguồn đèn: ${device.heaterSource} · Nguồn quạt: ${device.fanSource}",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }

        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(22.dp),
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
        ) {
            Column(
                modifier = Modifier.padding(18.dp),
                verticalArrangement = Arrangement.spacedBy(14.dp)
            ) {
                Text("Điều khiển hệ thống", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                Text("Nguồn máy ấp", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                ActionRow(
                    listOf(
                        "BẬT NGUỒN" to { onCommand(JSONObject().put("system_power", true)) },
                        "TẮT NGUỒN" to { onCommand(JSONObject().put("system_power", false)) }
                    )
                )
                Divider(color = MaterialTheme.colorScheme.outline)
                Text("Đèn sưởi", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                ActionRow(
                    listOf(
                        "BẬT" to { onCommand(JSONObject().put("heater_override", 1)) },
                        "TẮT" to { onCommand(JSONObject().put("heater_override", 0)) },
                        "AUTO" to { onCommand(JSONObject().put("heater_override", -1)) }
                    )
                )
                Text("Quạt", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                ActionRow(
                    listOf(
                        "BẬT" to { onCommand(JSONObject().put("fan_override", 1)) },
                        "TẮT" to { onCommand(JSONObject().put("fan_override", 0)) },
                        "AUTO" to { onCommand(JSONObject().put("fan_override", -1)) }
                    )
                )
                Text("Còi báo động", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                ActionRow(
                    listOf(
                        "CHO PHÉP" to { onCommand(JSONObject().put("buzzer_enabled", true)) },
                        "TẮT TIẾNG" to { onCommand(JSONObject().put("buzzer_enabled", false)) }
                    )
                )
            }
        }
        Spacer(Modifier.height(8.dp))
    }
}

// -----------------------------------------------------------------------------
// TAB 2 — nhật ký được diễn giải sang tiếng Việt trước khi hiển thị.
// -----------------------------------------------------------------------------
@Composable
private fun LogsTab(
    logs: List<JSONObject>,
    selectedCategory: Int,
    categoryMenuOpen: Boolean,
    onCategoryMenuOpenChange: (Boolean) -> Unit,
    onCategorySelected: (Int) -> Unit,
    onRefresh: () -> Unit,
    onDeleteGroup: () -> Unit,
    onDeleteLog: (JSONObject) -> Unit,
    notice: String,
    error: String
) {
    LazyColumn(
        modifier = Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background),
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        item {
            FeedbackCard(notice, error)
            Text("Nhật ký hoạt động", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
        }
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(22.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    CategorySelector(
                        selected = selectedCategory,
                        expanded = categoryMenuOpen,
                        setExpanded = onCategoryMenuOpenChange,
                        onSelected = onCategorySelected
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                        OutlinedButton(onClick = onRefresh, modifier = Modifier.weight(1f)) {
                            Text("LÀM MỚI", style = MaterialTheme.typography.labelLarge)
                        }
                        Button(onClick = onDeleteGroup, modifier = Modifier.weight(1f)) {
                            Text("XÓA NHÓM", style = MaterialTheme.typography.labelLarge)
                        }
                    }
                }
            }
        }
        if (logs.isEmpty()) {
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(22.dp),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
                ) {
                    Text(
                        "Không có nhật ký trong nhóm này.",
                        modifier = Modifier.padding(18.dp),
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        }
        items(logs, key = { it.optLong("id") }) { log ->
            LogCard(log = log, onDelete = { onDeleteLog(log) })
        }
        item { Spacer(Modifier.height(10.dp)) }
    }
}

/** Chỉ định dạng chuỗi ở tầng UI; JSONObject gốc và dữ liệu server không bị sửa. */
private fun humanizeLogMessage(raw: String): String {
    var message = raw.trim()
    if (message.isBlank()) return "Đã ghi nhận hoạt động thiết bị"

    message = message.replace(Regex("(?i)^LOG\\[\\d+]\\s*"), "")
        .replace(Regex("^\\[[^]]+]\\s*"), "")
        .replace(Regex("(?i)^Lệnh quản trị:\\s*"), "")
        .trim()

    val commandPattern = Regex(
        "(?i)\\b(system_power|heater_override|fan_override|buzzer_enabled|mq2_alarm_enabled)\\s*[:=]\\s*(true|false|-?\\d+)"
    )
    message = commandPattern.replace(message) { match ->
        val key = match.groupValues[1].lowercase(Locale.ROOT)
        val value = match.groupValues[2].lowercase(Locale.ROOT)
        when (key) {
            "system_power" -> if (value == "true" || value == "1") "Hệ thống đã bật" else "Hệ thống đã tắt"
            "heater_override" -> when (value) {
                "1", "true" -> "Đèn sưởi được bật thủ công"
                "0", "false" -> "Đèn sưởi được tắt thủ công"
                else -> "Đèn sưởi trở về chế độ tự động"
            }
            "fan_override" -> when (value) {
                "1", "true" -> "Quạt được bật thủ công"
                "0", "false" -> "Quạt được tắt thủ công"
                else -> "Quạt trở về chế độ tự động"
            }
            "buzzer_enabled" -> if (value == "true" || value == "1") "Đã cho phép còi báo" else "Đã tắt tiếng còi"
            "mq2_alarm_enabled" -> if (value == "true" || value == "1") "Đã bật cảnh báo khí gas" else "Đã tắt cảnh báo khí gas"
            else -> "Đã cập nhật trạng thái thiết bị"
        }
    }

    val translations = listOf(
        "Firmware khoi dong" to "Thiết bị vừa khởi động",
        "DHT11 da co du lieu hop le" to "Cảm biến nhiệt độ và độ ẩm hoạt động bình thường",
        "NHIET DO THAP" to "Nhiệt độ đang thấp",
        "CO KHI DOC! MQ2 vuot nguong ADC" to "Phát hiện khí bất thường",
        "ESP32 ket noi router Wi-Fi" to "Thiết bị đã kết nối Wi-Fi",
        "Mat Wi-Fi router" to "Thiết bị mất kết nối Wi-Fi",
        "Da ket noi MQTT cloud" to "Đã kết nối máy chủ cloud",
        "Den suoi BAT" to "Đèn sưởi đã bật",
        "Den suoi TAT" to "Đèn sưởi đã tắt",
        "Quat BAT" to "Quạt đã bật",
        "Quat TAT" to "Quạt đã tắt",
        "Tat tieng coi" to "Đã tắt tiếng còi",
        "BAT tieng coi" to "Đã bật còi báo",
        "HE THONG: BAT" to "Hệ thống đã bật",
        "HE THONG: TAT" to "Hệ thống đã tắt"
    )
    translations.forEach { (source, target) ->
        message = message.replace(source, target, ignoreCase = true)
    }

    message = message
        .replace(Regex("(?i)\\btrue\\b"), "đã bật")
        .replace(Regex("(?i)\\bfalse\\b"), "đã tắt")
        .replace(Regex("[{}\\[\\]\\\"]"), " ")
        .replace(Regex("\\s+"), " ")
        .trim()

    // Tránh để lộ tên biến dạng code nếu log có khóa chưa nằm trong từ điển.
    message = message.replace(
        Regex("\\b[a-zA-Z]+_[a-zA-Z0-9_]+\\b"),
        "trạng thái thiết bị"
    )
    return message.ifBlank { "Đã ghi nhận hoạt động thiết bị" }
}

// -----------------------------------------------------------------------------
// TAB 3 — phiên quản trị trước, cấu hình Wi-Fi Bluetooth nằm cuối trang.
// -----------------------------------------------------------------------------
@Composable
private fun SettingsTab(
    wifiSsid: String,
    onWifiSsidChange: (String) -> Unit,
    wifiPassword: String,
    onWifiPasswordChange: (String) -> Unit,
    bluetoothSetupPassword: String,
    onBluetoothPasswordChange: (String) -> Unit,
    isSendingWifi: Boolean,
    onSendWifi: () -> Unit,
    onLogout: () -> Unit,
    notice: String,
    error: String
) {
    LazyColumn(
        modifier = Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background),
        contentPadding = PaddingValues(16.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp)
    ) {
        item {
            FeedbackCard(notice, error)
            Text("Cài đặt", style = MaterialTheme.typography.headlineSmall, fontWeight = FontWeight.Bold)
        }
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(22.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text("Phiên quản trị", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                    OutlinedButton(
                        onClick = onLogout,
                        modifier = Modifier.fillMaxWidth(),
                        shape = RoundedCornerShape(24.dp)
                    ) {
                        Text("ĐĂNG XUẤT", style = MaterialTheme.typography.labelLarge)
                    }
                }
            }
        }
        // Khối cấu hình mạng được đặt cuối cùng theo đặc tả.
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(22.dp),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
            ) {
                Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text("Cấu hình Wi-Fi cho ESP32", style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Bold)
                    OutlinedTextField(
                        value = wifiSsid,
                        onValueChange = onWifiSsidChange,
                        label = { Text("Tên Wi-Fi (SSID)") },
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        shape = RoundedCornerShape(16.dp)
                    )
                    OutlinedTextField(
                        value = wifiPassword,
                        onValueChange = onWifiPasswordChange,
                        label = { Text("Mật khẩu Wi-Fi") },
                        visualTransformation = PasswordVisualTransformation(),
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        shape = RoundedCornerShape(16.dp)
                    )
                    OutlinedTextField(
                        value = bluetoothSetupPassword,
                        onValueChange = onBluetoothPasswordChange,
                        label = { Text("Mật khẩu cấu hình Bluetooth") },
                        visualTransformation = PasswordVisualTransformation(),
                        modifier = Modifier.fillMaxWidth(),
                        singleLine = true,
                        shape = RoundedCornerShape(16.dp)
                    )
                    Button(
                        onClick = onSendWifi,
                        enabled = !isSendingWifi && wifiSsid.isNotBlank() && bluetoothSetupPassword.isNotBlank(),
                        modifier = Modifier.fillMaxWidth().height(52.dp),
                        shape = RoundedCornerShape(26.dp)
                    ) {
                        Text(if (isSendingWifi) "ĐANG GỬI..." else "GỬI CẤU HÌNH QUA BLUETOOTH")
                    }
                }
            }
        }
        item { Spacer(Modifier.height(8.dp)) }
    }
}

// -----------------------------------------------------------------------------
// COMPONENT DÙNG CHUNG — màu sắc dùng token darkColorScheme.
// -----------------------------------------------------------------------------
@Composable
private fun MetricCard(
    title: String,
    value: String,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier,
        shape = RoundedCornerShape(22.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
    ) {
        Column(
            modifier = Modifier.fillMaxWidth().padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(9.dp)
        ) {
            Text(
                title,
                style = MaterialTheme.typography.labelLarge,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
            Text(
                value,
                style = MaterialTheme.typography.headlineSmall,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.onSurface
            )
        }
    }
}

@Composable
private fun StatusLine(label: String, value: String, good: Boolean) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(label, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Text(
            value,
            style = MaterialTheme.typography.labelLarge,
            color = if (good) Color(0xFF31C48D) else MaterialTheme.colorScheme.error
        )
    }
}

@Composable
private fun ActionRow(actions: List<Pair<String, () -> Unit>>) {
    Row(
        modifier = Modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        actions.forEach { (label, action) ->
            OutlinedButton(
                onClick = action,
                modifier = Modifier.weight(1f),
                contentPadding = PaddingValues(horizontal = 7.dp, vertical = 12.dp),
                shape = RoundedCornerShape(18.dp)
            ) {
                Text(
                    label,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    style = MaterialTheme.typography.labelLarge,
                    fontWeight = FontWeight.SemiBold
                )
            }
        }
    }
}

@Composable
private fun FeedbackCard(notice: String, error: String) {
    if (notice.isNotBlank() || error.isNotBlank()) {
        Card(
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(16.dp),
            colors = CardDefaults.cardColors(
                containerColor = if (error.isNotBlank()) Color(0xFF3A1D1D) else Color(0xFF123326)
            )
        ) {
            Text(
                text = if (error.isNotBlank()) error else notice,
                modifier = Modifier.padding(14.dp),
                color = if (error.isNotBlank()) Color(0xFFFFB4AB) else Color(0xFF9BE5C1),
                style = MaterialTheme.typography.bodyMedium
            )
        }
    }
}

@Composable
private fun CategorySelector(
    selected: Int,
    expanded: Boolean,
    setExpanded: (Boolean) -> Unit,
    onSelected: (Int) -> Unit
) {
    Box {
        OutlinedButton(
            onClick = { setExpanded(true) },
            modifier = Modifier.fillMaxWidth(),
            shape = RoundedCornerShape(18.dp)
        ) {
            Text(logCategories.getOrElse(selected + 1) { "Tất cả nhật ký" })
        }
        DropdownMenu(expanded = expanded, onDismissRequest = { setExpanded(false) }) {
            logCategories.forEachIndexed { index, label ->
                DropdownMenuItem(
                    text = { Text(label) },
                    onClick = {
                        onSelected(index - 1)
                        setExpanded(false)
                    }
                )
            }
        }
    }
}

@Composable
private fun LogCard(log: JSONObject, onDelete: () -> Unit) {
    val timestamp = log.optLong("created_at")
    val formatted = if (timestamp > 0) {
        SimpleDateFormat("dd/MM/yyyy HH:mm:ss", Locale.getDefault()).format(Date(timestamp))
    } else "Chưa đồng bộ giờ"
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(20.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(start = 15.dp, top = 13.dp, bottom = 13.dp, end = 8.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Text(formatted, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Text(
                    logCategories.getOrElse(log.optInt("category") + 1) { "Hệ thống" },
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.primary
                )
                Text(
                    humanizeLogMessage(log.optString("message")),
                    style = MaterialTheme.typography.bodyLarge,
                    color = MaterialTheme.colorScheme.onSurface
                )
            }
            TextButton(onClick = onDelete) { Text("XÓA", color = MaterialTheme.colorScheme.primary) }
        }
    }
}

// -----------------------------------------------------------------------------
// GỬI LỆNH — giữ nguyên endpoint /api/control và JSON command của mã gốc.
// -----------------------------------------------------------------------------
private fun runCommand(
    baseUrl: String,
    token: String?,
    command: JSONObject,
    scope: kotlinx.coroutines.CoroutineScope,
    setMessage: (String, Boolean) -> Unit
) {
    scope.launch {
        try {
            if (token == null) error("Phiên đăng nhập đã hết hạn.")
            ApiClient.request(baseUrl, "/api/control", "POST", token, command)
            setMessage("Đã gửi lệnh đến ESP32.", false)
        } catch (e: Exception) {
            setMessage(e.message ?: "Không gửi được lệnh.", true)
        }
    }
}

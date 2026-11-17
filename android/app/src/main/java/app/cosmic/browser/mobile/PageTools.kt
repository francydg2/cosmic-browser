package app.cosmic.browser.mobile

import com.google.zxing.BarcodeFormat
import com.google.zxing.EncodeHintType
import com.google.zxing.qrcode.QRCodeWriter

internal fun matchesTab(title: String, url: String, query: String): Boolean {
    val term = query.trim()
    return title.contains(term, ignoreCase = true) || url.contains(term, ignoreCase = true)
}

internal fun pageQrCode(url: String) = QRCodeWriter().encode(
    url, BarcodeFormat.QR_CODE, 600, 600,
    mapOf(EncodeHintType.CHARACTER_SET to "UTF-8", EncodeHintType.MARGIN to 4)
)

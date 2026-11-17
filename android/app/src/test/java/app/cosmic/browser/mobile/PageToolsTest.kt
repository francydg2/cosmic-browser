package app.cosmic.browser.mobile

import com.google.zxing.BinaryBitmap
import com.google.zxing.RGBLuminanceSource
import com.google.zxing.WriterException
import com.google.zxing.common.HybridBinarizer
import com.google.zxing.qrcode.QRCodeReader
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class PageToolsTest {
    @Test
    fun searchesTitlesAndAddressesIgnoringCaseAndWhitespace() {
        assertTrue(matchesTab("Cosmic Browser", "https://example.org", " cosmic "))
        assertTrue(matchesTab("Example", "https://example.org/News", "NEWS"))
        assertFalse(matchesTab("Example", "https://example.org", "missing"))
        assertTrue(matchesTab("Example", "https://example.org", " "))
    }

    @Test
    fun qrCodeRoundTripsUnicodeAddress() {
        val url = "https://example.org/ricerca?q=caff%C3%A8&lang=it"
        val matrix = pageQrCode(url)
        val pixels = IntArray(matrix.width * matrix.height) { index ->
            if (matrix[index % matrix.width, index / matrix.width]) 0xff000000.toInt()
            else 0xffffffff.toInt()
        }
        val source = RGBLuminanceSource(matrix.width, matrix.height, pixels)
        val result = QRCodeReader().decode(BinaryBitmap(HybridBinarizer(source)))
        assertEquals(url, result.text)
    }

    @Test(expected = WriterException::class)
    fun rejectsAddressesTooLongForQrCode() {
        pageQrCode("https://example.org/" + "a".repeat(10000))
    }
}

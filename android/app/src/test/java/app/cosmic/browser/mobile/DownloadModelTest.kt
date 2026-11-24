package app.cosmic.browser.mobile

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class DownloadModelTest {
    @Test
    fun filtersMatchStates() {
        assertTrue(DownloadFilter.ALL.accepts(DownloadState.CANCELLED))
        assertTrue(DownloadFilter.ACTIVE.accepts(DownloadState.RUNNING))
        assertFalse(DownloadFilter.ACTIVE.accepts(DownloadState.DONE))
        assertTrue(DownloadFilter.FAILED.accepts(DownloadState.FAILED))
        assertTrue(DownloadFilter.FAILED.accepts(DownloadState.CANCELLED))
        assertFalse(DownloadFilter.DONE.accepts(DownloadState.FAILED))
    }

    @Test
    fun searchMatchesNameOrAddressIgnoringCase() {
        assertTrue(matchesQuery("Report.PDF", "https://a.test/x", "report"))
        assertTrue(matchesQuery("a.bin", "https://Files.Test/a.bin", "files.test"))
        assertTrue(matchesQuery("a.bin", "https://x", "  "))
        assertFalse(matchesQuery("a.bin", "https://x", "zzz"))
    }

    @Test
    fun speedUsesARollingWindow() {
        val tracker = SpeedTracker()
        assertEquals(0.0, tracker.bytesPerSecond(), 0.0)
        tracker.sample(0, 0)
        tracker.sample(2000, 2000)
        assertEquals(1000.0, tracker.bytesPerSecond(), 0.001)
        assertEquals(8L, tracker.secondsRemaining(10000, 2000))
        assertEquals(-1L, tracker.secondsRemaining(-1, 2000))
        tracker.sample(20000, 2000) // stalled: the old burst leaves the window
        assertEquals(0.0, tracker.bytesPerSecond(), 0.0)
        tracker.sample(21000, 100)  // counter went backwards: restart
        assertTrue(tracker.bytesPerSecond() >= 0.0)
    }

    @Test
    fun savedListRoundTripsAndMarksRunningAsInterrupted() {
        val saved = listOf(
            SavedDownload(1, "a.pdf", "https://x/a.pdf", "application/pdf", DownloadState.DONE, 100, 100,
                "content://media/1", "", 5),
            SavedDownload(2, "b.bin", "https://x/b", null, DownloadState.FAILED, -1, 3, null, "HTTP 500", 6),
            SavedDownload(3, "c.bin", "https://x/c", null, DownloadState.RUNNING, 10, 1, null, "", 0)
        )
        val loaded = savedFromJson(savedToJson(saved))
        assertEquals(3, loaded.size)
        assertEquals("content://media/1", loaded[0].uri)
        assertNull(loaded[1].mime)
        assertEquals("HTTP 500", loaded[1].error)
        assertEquals(DownloadState.FAILED, loaded[2].state)
        assertTrue(loaded[2].error.isNotEmpty())
    }

    @Test
    fun corruptOrHostileFilesAreIgnored() {
        assertTrue(savedFromJson("not json").isEmpty())
        assertTrue(savedFromJson("{}").isEmpty())
        val text = """{"downloads":[{"name":"","state":"DONE"},{"name":"x","state":"WAT"},{"name":"ok","state":"DONE","id":9}]}"""
        val loaded = savedFromJson(text)
        assertEquals(1, loaded.size)
        assertEquals("ok", loaded[0].name)
    }

    @Test
    fun formatsSizesAndTimes() {
        assertEquals("512 B", formatSize(512))
        assertEquals("1,5 KB", formatSize(1536))
        assertEquals("", formatSpeed(0.5))
        assertEquals("2 min 5 s", formatDuration(125))
        assertEquals("—", formatDuration(-1))
        assertEquals("1 h 2 min", formatDuration(3725))
    }
}

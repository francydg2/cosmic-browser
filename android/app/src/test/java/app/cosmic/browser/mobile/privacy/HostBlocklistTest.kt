package app.cosmic.browser.mobile.privacy

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class HostBlocklistTest {
    private val blocker = HostBlocklist(sequenceOf("# comment", " Tracker.Example ", ""))

    @Test
    fun blocksExactHostsAndSubdomains() {
        assertTrue(blocker.blocks("TRACKER.EXAMPLE"))
        assertTrue(blocker.blocks("cdn.tracker.example."))
    }

    @Test
    fun doesNotBlockNearMissesOrMissingHosts() {
        assertFalse(blocker.blocks("nottracker.example"))
        assertFalse(blocker.blocks("tracker.example.evil"))
        assertFalse(blocker.blocks(null))
    }
}

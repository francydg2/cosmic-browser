package app.cosmic.browser.mobile

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.json.JSONArray
import org.json.JSONObject
import java.io.IOException

class UpdateMetadataTest {
    @Test
    fun comparesBetasAndStableVersions() {
        val beta1 = UpdateVersion.parse("0.1.3-beta")!!
        val beta2 = UpdateVersion.parse("0.1.3-beta2")!!
        val stable = UpdateVersion.parse("0.1.3")!!
        val next = UpdateVersion.parse("0.1.4-beta1")!!
        assertTrue(beta1 < beta2)
        assertTrue(beta2 < stable)
        assertTrue(stable < next)
        assertNull(UpdateVersion.parse("0.1.3-beta0"))
        assertNull(UpdateVersion.parse("0.1.3-beta2147483647"))
        assertNull(UpdateVersion.parse("0.1.3-preview"))
    }

    @Test
    fun acceptsOnlyExpectedReleaseAssetWithHashAndSize() {
        val name = "cosmic-0.1.3-android-beta3.apk"
        val url = "https://github.com/francydg2/cosmic-browser/releases/download/v0.1.3/$name"
        val digest = "sha256:" + "ab".repeat(32)
        val candidate = candidateFromAsset("v0.1.3", name, url, digest, 100)
        assertNotNull(candidate)
        assertEquals("0.1.3-beta3", candidate!!.versionName)
        assertEquals("ab".repeat(32), candidate.sha256)
        assertNull(candidateFromAsset("v0.1.2", name, url, digest, 100))
        assertNull(candidateFromAsset("v0.1.3", name, "$url?redirect=evil", digest, 100))
        assertNull(candidateFromAsset("v0.1.3", name, url.replace("github.com", "github.com.evil.test"), digest, 100))
        assertNull(candidateFromAsset("v0.1.3", name, url, "", 100))
        assertNull(candidateFromAsset("v0.1.3", name, url, digest, MAX_UPDATE_BYTES + 1))
        assertFalse(UpdateVersion.parse("0.1.3-beta3")!! > candidate.version)
    }

    @Test
    fun findsNewestAndroidReleaseIncludingBetaAndSkipsDrafts() {
        val releases = JSONArray()
            .put(release("v0.1.3", "beta3"))
            .put(release("v0.1.3", "beta4"))
            .put(release("v0.1.4", "beta1").put("draft", true))
        val current = UpdateVersion.parse("0.1.3-beta3")!!
        assertEquals("0.1.3-beta4", newestUpdate(releases.toString(), current)?.versionName)
        assertNull(newestUpdate(releases.toString(), UpdateVersion.parse("0.1.3-beta4")!!))
    }

    @Test(expected = IOException::class)
    fun rejectsNewerReleaseWithoutVerifiableDigest() {
        val invalid = release("v0.1.3", "beta4")
        invalid.getJSONArray("assets").getJSONObject(0).remove("digest")
        newestUpdate(JSONArray().put(invalid).toString(), UpdateVersion.parse("0.1.3-beta3")!!)
    }

    @Test(expected = IOException::class)
    fun rejectsDuplicateNewerPackages() {
        val releases = JSONArray().put(release("v0.1.3", "beta4")).put(release("v0.1.3", "beta4"))
        newestUpdate(releases.toString(), UpdateVersion.parse("0.1.3-beta3")!!)
    }

    private fun release(tag: String, beta: String): JSONObject {
        val base = tag.removePrefix("v")
        val name = "cosmic-$base-android-$beta.apk"
        val asset = JSONObject().put("name", name)
            .put("browser_download_url", "https://github.com/francydg2/cosmic-browser/releases/download/$tag/$name")
            .put("digest", "sha256:" + "ab".repeat(32))
            .put("size", 100)
        return JSONObject().put("tag_name", tag).put("assets", JSONArray().put(asset))
    }
}

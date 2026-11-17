package app.cosmic.browser.mobile

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SearchTemplateTest {
    @Test
    fun acceptsWebTemplatesWithAPlaceholder() {
        assertTrue(validSearchTemplate("https://example.org/search?q=%1"))
        assertTrue(validSearchTemplate("http://example.org/%1"))
    }

    @Test
    fun rejectsInvalidOrUnsafeTemplates() {
        assertFalse(validSearchTemplate("https://example.org/search"))
        assertFalse(validSearchTemplate("javascript:alert(%1)"))
        assertFalse(validSearchTemplate("https://user:pass@example.org/?q=%1"))
        assertFalse(validSearchTemplate("https:///?q=%1"))
        assertFalse(validSearchTemplate("https://example.org/?q=%1&bad=%xx"))
    }
}

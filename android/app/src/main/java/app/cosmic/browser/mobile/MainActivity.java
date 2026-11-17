package app.cosmic.browser.mobile;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.content.Context;
import android.webkit.CookieManager;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebStorage;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.GridLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

public final class MainActivity extends Activity {
    private static final int BG = Color.rgb(17, 19, 25);
    private static final int SURFACE = Color.rgb(21, 24, 32);
    private static final int CARD = Color.rgb(26, 30, 39);
    private static final int FIELD = Color.rgb(32, 36, 45);
    private static final int BORDER = Color.rgb(52, 59, 72);
    private static final int TEXT = Color.rgb(232, 233, 238);
    private static final int MUTED = Color.rgb(152, 162, 178);
    private static final int ACCENT = Color.rgb(124, 108, 240);
    private static final String PREFS = "cosmic_mobile";
    private static final String[] ENGINE_IDS = {"duckduckgo", "google", "bing", "brave"};
    private static final String[] ENGINE_NAMES = {"DuckDuckGo", "Google", "Bing", "Brave Search"};
    private static final String[] ENGINE_URLS = {
            "https://duckduckgo.com/?q=%s",
            "https://www.google.com/search?q=%s",
            "https://www.bing.com/search?q=%s",
            "https://search.brave.com/search?q=%s"
    };

    private final List<BrowserTab> tabs = new ArrayList<>();
    private SharedPreferences preferences;
    private LinearLayout root;
    private FrameLayout content;
    private EditText addressField;
    private BrowserTab activeTab;
    private Screen screen = Screen.BROWSER;

    private enum Screen { BROWSER, TABS, BOOKMARKS, SETTINGS }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().setStatusBarColor(BG);
        getWindow().setNavigationBarColor(BG);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(true);
        }
        preferences = getSharedPreferences(PREFS, MODE_PRIVATE);
        root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        setContentView(root);
        restoreTabs();
        render();
    }

    private void restoreTabs() {
        String saved = preferences.getString("tabs", "[]");
        try {
            JSONArray urls = new JSONArray(saved);
            for (int i = 0; i < urls.length() && i < 20; i++) {
                String url = urls.optString(i, "");
                tabs.add(new BrowserTab(url));
            }
            if (!tabs.isEmpty()) {
                int index = Math.max(0, Math.min(preferences.getInt("activeTab", 0), tabs.size() - 1));
                activeTab = tabs.get(index);
            }
        } catch (JSONException ignored) {
            preferences.edit().remove("tabs").apply();
        }
        if (activeTab == null) {
            activeTab = new BrowserTab("");
            tabs.add(activeTab);
        }
        for (BrowserTab tab : tabs) {
            createWebView(tab);
            if (!tab.url.isEmpty()) {
                tab.webView.loadUrl(tab.url);
            }
        }
    }

    private void createTab(String url) {
        BrowserTab tab = new BrowserTab(url);
        createWebView(tab);
        tabs.add(tab);
        activeTab = tab;
        screen = Screen.BROWSER;
        render();
        if (!url.isEmpty()) {
            tab.webView.loadUrl(url);
        }
    }

    private void createWebView(BrowserTab tab) {
        WebView webView = new WebView(this);
        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setAllowFileAccess(false);
        settings.setAllowContentAccess(false);
        settings.setJavaScriptCanOpenWindowsAutomatically(false);
        settings.setSupportMultipleWindows(false);
        settings.setMixedContentMode(WebSettings.MIXED_CONTENT_NEVER_ALLOW);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            settings.setSafeBrowsingEnabled(true);
        }

        webView.setBackgroundColor(BG);
        webView.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                return handleExternalScheme(request.getUrl().toString());
            }

            @Override
            public boolean shouldOverrideUrlLoading(WebView view, String url) {
                return handleExternalScheme(url);
            }

            @Override
            public void onPageStarted(WebView view, String url, android.graphics.Bitmap favicon) {
                if ("about:blank".equals(url)) {
                    return;
                }
                tab.url = url;
                if (tab == activeTab) {
                    updateChrome();
                }
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                if ("about:blank".equals(url)) {
                    return;
                }
                tab.url = url;
                String title = view.getTitle();
                if (title != null && !title.trim().isEmpty()) {
                    tab.title = title;
                }
                if (tab == activeTab) {
                    updateChrome();
                }
            }
        });
        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onReceivedTitle(WebView view, String title) {
                if (title != null && !title.trim().isEmpty()) {
                    tab.title = title;
                    if (tab == activeTab) {
                        updateChrome();
                    }
                }
            }
        });
        tab.webView = webView;
    }

    private boolean handleExternalScheme(String value) {
        Uri uri = Uri.parse(value);
        String scheme = uri.getScheme();
        if ("http".equalsIgnoreCase(scheme) || "https".equalsIgnoreCase(scheme)
                || "about".equalsIgnoreCase(scheme)) {
            return false;
        }
        try {
            startActivity(new Intent(Intent.ACTION_VIEW, uri));
        } catch (ActivityNotFoundException exception) {
            Toast.makeText(this, "Nessuna app disponibile per questo link", Toast.LENGTH_SHORT).show();
        }
        return true;
    }

    private void render() {
        root.removeAllViews();
        root.addView(buildHeader(), new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(54)));

        if (screen == Screen.BROWSER) {
            root.addView(buildAddressBar(), new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, dp(58)));
        } else if (screen == Screen.TABS) {
            root.addView(buildTabsHeading(), new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, dp(56)));
        }

        content = new FrameLayout(this);
        root.addView(content, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));

        switch (screen) {
            case BROWSER:
                renderBrowser();
                root.addView(buildNavigationBar(), new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, dp(62)));
                break;
            case TABS:
                renderTabs();
                root.addView(buildNavigationBar(), new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, dp(62)));
                break;
            case BOOKMARKS:
                renderBookmarks();
                break;
            case SETTINGS:
                renderSettings();
                break;
        }
    }

    private View buildHeader() {
        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(18), 0, dp(14), 0);
        row.setBackgroundColor(BG);

        if (screen == Screen.SETTINGS || screen == Screen.BOOKMARKS) {
            row.addView(textButton("‹", "Indietro", () -> {
                screen = Screen.BROWSER;
                render();
            }), new LinearLayout.LayoutParams(dp(42), dp(42)));
            TextView title = label(screen == Screen.SETTINGS ? "Impostazioni" : "Preferiti", 18, TEXT, true);
            LinearLayout.LayoutParams titleParams = new LinearLayout.LayoutParams(0,
                    ViewGroup.LayoutParams.WRAP_CONTENT, 1);
            titleParams.leftMargin = dp(8);
            row.addView(title, titleParams);
            return row;
        }

        TextView brand = label("COSMIC", 16, TEXT, true);
        brand.setLetterSpacing(0.12f);
        row.addView(brand, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        if (screen == Screen.BROWSER) {
            row.addView(textButton(isBookmarked(activeTab.url) ? "★" : "☆", "Aggiungi ai preferiti",
                    this::toggleBookmark), new LinearLayout.LayoutParams(dp(42), dp(42)));
            row.addView(textButton("Menu", "Apri il menu", this::showMenu),
                    new LinearLayout.LayoutParams(dp(58), dp(42)));
        } else {
            row.addView(textButton("+", "Nuova scheda", () -> createTab("")),
                    new LinearLayout.LayoutParams(dp(42), dp(42)));
        }
        return row;
    }

    private View buildAddressBar() {
        LinearLayout outer = new LinearLayout(this);
        outer.setGravity(Gravity.CENTER_VERTICAL);
        outer.setPadding(dp(14), dp(5), dp(14), dp(7));

        LinearLayout pill = new LinearLayout(this);
        pill.setGravity(Gravity.CENTER_VERTICAL);
        pill.setPadding(dp(13), 0, dp(6), 0);
        pill.setBackground(rounded(FIELD, BORDER, 24));

        TextView secure = label(activeTab.url.startsWith("https://") ? "●" : "◎", 13,
                activeTab.url.startsWith("https://") ? Color.rgb(135, 214, 174) : MUTED, true);
        pill.addView(secure, new LinearLayout.LayoutParams(dp(22), ViewGroup.LayoutParams.WRAP_CONTENT));

        addressField = new EditText(this);
        addressField.setSingleLine(true);
        addressField.setTextColor(TEXT);
        addressField.setHintTextColor(MUTED);
        addressField.setTextSize(14);
        addressField.setHint("Cerca o inserisci un indirizzo");
        addressField.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
        addressField.setImeOptions(EditorInfo.IME_ACTION_GO);
        addressField.setBackgroundColor(Color.TRANSPARENT);
        addressField.setPadding(dp(5), dp(10), dp(5), dp(10));
        addressField.setSelectAllOnFocus(true);
        addressField.setText(activeTab.url);
        pill.addView(addressField, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));

        TextView go = textButton("↵", "Vai", () -> navigate(addressField.getText().toString()));
        go.setTextColor(ACCENT);
        pill.addView(go, new LinearLayout.LayoutParams(dp(40), dp(40)));
        addressField.setOnEditorActionListener((view, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_GO || actionId == EditorInfo.IME_ACTION_DONE
                    || (event != null && event.getKeyCode() == KeyEvent.KEYCODE_ENTER)) {
                navigate(addressField.getText().toString());
                return true;
            }
            return false;
        });
        outer.addView(pill, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(48)));
        return outer;
    }

    private View buildTabsHeading() {
        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(dp(18), 0, dp(16), 0);
        TextView heading = label(tabs.size() + " schede", 18, TEXT, true);
        row.addView(heading, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        row.addView(textButton("Chiudi tutte", "Chiudi tutte le schede", () -> {
            for (BrowserTab tab : new ArrayList<>(tabs)) {
                destroyTab(tab);
            }
            tabs.clear();
            createTab("");
        }), new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(42)));
        return row;
    }

    private View buildNavigationBar() {
        LinearLayout bar = new LinearLayout(this);
        bar.setGravity(Gravity.CENTER);
        bar.setPadding(dp(12), dp(5), dp(12), dp(8));
        bar.setBackgroundColor(SURFACE);
        bar.addView(textButton("←", "Indietro", this::goBack),
                new LinearLayout.LayoutParams(0, dp(46), 1));
        bar.addView(textButton("→", "Avanti", this::goForward),
                new LinearLayout.LayoutParams(0, dp(46), 1));
        bar.addView(textButton("⌂", "Home", () -> {
            showHome();
        }), new LinearLayout.LayoutParams(0, dp(46), 1));
        bar.addView(textButton(Integer.toString(tabs.size()), "Schede aperte", () -> {
            screen = Screen.TABS;
            render();
        }), new LinearLayout.LayoutParams(0, dp(46), 1));
        ImageView settingsButton = new ImageView(this);
        settingsButton.setImageResource(R.drawable.ic_settings);
        settingsButton.setContentDescription("Impostazioni");
        settingsButton.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        settingsButton.setPadding(dp(12), dp(12), dp(12), dp(12));
        settingsButton.setBackground(rounded(Color.TRANSPARENT, Color.TRANSPARENT, 14));
        settingsButton.setOnClickListener(v -> {
            screen = Screen.SETTINGS;
            render();
        });
        settingsButton.setFocusable(true);
        bar.addView(settingsButton, new LinearLayout.LayoutParams(0, dp(46), 1));
        return bar;
    }

    private void renderBrowser() {
        if (activeTab.url.isEmpty()) {
            content.addView(buildHomePage(), matchParent());
            return;
        }
        if (activeTab.webView.getParent() instanceof ViewGroup) {
            ((ViewGroup) activeTab.webView.getParent()).removeView(activeTab.webView);
        }
        content.addView(activeTab.webView, matchParent());
        activeTab.webView.onResume();
    }

    private View buildHomePage() {
        ScrollView scroll = new ScrollView(this);
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setGravity(Gravity.CENTER_HORIZONTAL);
        page.setPadding(dp(24), dp(44), dp(24), dp(30));

        TextView planet = label("◉", 48, ACCENT, true);
        planet.setGravity(Gravity.CENTER);
        page.addView(planet, new LinearLayout.LayoutParams(dp(92), dp(92)));

        TextView title = label("Il web, al tuo ritmo.", 24, TEXT, true);
        title.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams titleParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        titleParams.topMargin = dp(12);
        page.addView(title, titleParams);

        TextView subtitle = label("Cerca qualcosa o apri un nuovo spazio.", 14, MUTED, false);
        subtitle.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams subtitleParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        subtitleParams.topMargin = dp(8);
        page.addView(subtitle, subtitleParams);

        List<Bookmark> bookmarks = loadBookmarks();
        if (!bookmarks.isEmpty()) {
            TextView section = label("I tuoi preferiti", 14, TEXT, true);
            LinearLayout.LayoutParams sectionParams = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            sectionParams.topMargin = dp(32);
            page.addView(section, sectionParams);
            GridLayout grid = new GridLayout(this);
            grid.setColumnCount(2);
            for (Bookmark bookmark : bookmarks) {
                TextView tile = label(bookmark.title, 14, TEXT, true);
                tile.setGravity(Gravity.CENTER);
                tile.setPadding(dp(10), dp(14), dp(10), dp(14));
                tile.setBackground(rounded(CARD, BORDER, 14));
                tile.setOnClickListener(v -> navigate(bookmark.url));
                GridLayout.LayoutParams params = new GridLayout.LayoutParams();
                params.width = 0;
                params.columnSpec = GridLayout.spec(GridLayout.UNDEFINED, 1f);
                params.setMargins(dp(4), dp(4), dp(4), dp(4));
                grid.addView(tile, params);
            }
            page.addView(grid, new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        } else {
            TextView hint = label("I tuoi siti salvati compariranno qui.", 13, MUTED, false);
            hint.setGravity(Gravity.CENTER);
            LinearLayout.LayoutParams hintParams = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            hintParams.topMargin = dp(32);
            page.addView(hint, hintParams);
        }
        scroll.setFillViewport(true);
        scroll.addView(page);
        return scroll;
    }

    private void renderTabs() {
        ScrollView scroll = new ScrollView(this);
        GridLayout grid = new GridLayout(this);
        grid.setColumnCount(2);
        grid.setPadding(dp(12), dp(8), dp(12), dp(24));
        for (BrowserTab tab : tabs) {
            GridLayout.LayoutParams params = new GridLayout.LayoutParams();
            params.width = 0;
            params.height = ViewGroup.LayoutParams.WRAP_CONTENT;
            params.columnSpec = GridLayout.spec(GridLayout.UNDEFINED, 1f);
            params.setMargins(dp(6), dp(6), dp(6), dp(6));
            grid.addView(buildTabCard(tab), params);
        }
        scroll.addView(grid);
        content.addView(scroll, matchParent());
    }

    private View buildTabCard(BrowserTab tab) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(12), dp(12), dp(12), dp(12));
        card.setBackground(rounded(tab == activeTab ? Color.rgb(38, 35, 60) : CARD,
                tab == activeTab ? ACCENT : BORDER, 16));

        LinearLayout top = new LinearLayout(this);
        top.setGravity(Gravity.CENTER_VERTICAL);
        TextView domain = label(tab.url.isEmpty() ? "COSMIC" : domain(tab.url), 12, MUTED, true);
        top.addView(domain, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        top.addView(textButton("×", "Chiudi scheda", () -> closeTab(tab)),
                new LinearLayout.LayoutParams(dp(34), dp(34)));
        card.addView(top);

        TextView preview = label(tab.url.isEmpty() ? "◉" : " " + tab.title, 16, TEXT, true);
        preview.setGravity(Gravity.CENTER);
        preview.setPadding(dp(6), dp(14), dp(6), dp(14));
        preview.setMinHeight(dp(88));
        preview.setBackground(rounded(FIELD, Color.TRANSPARENT, 12));
        LinearLayout.LayoutParams previewParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(92));
        previewParams.topMargin = dp(8);
        card.addView(preview, previewParams);

        TextView title = label(tab.title, 14, TEXT, true);
        title.setMaxLines(1);
        LinearLayout.LayoutParams titleParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        titleParams.topMargin = dp(10);
        card.addView(title, titleParams);

        TextView url = label(tab.url.isEmpty() ? "Nuova scheda" : tab.url, 11, MUTED, false);
        url.setMaxLines(1);
        LinearLayout.LayoutParams urlParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        urlParams.topMargin = dp(4);
        card.addView(url, urlParams);

        card.setOnClickListener(v -> {
            activeTab = tab;
            screen = Screen.BROWSER;
            render();
        });
        return card;
    }

    private void renderBookmarks() {
        ScrollView scroll = new ScrollView(this);
        LinearLayout list = new LinearLayout(this);
        list.setOrientation(LinearLayout.VERTICAL);
        list.setPadding(dp(18), dp(12), dp(18), dp(28));

        TextView add = label("＋  Salva pagina corrente", 14, TEXT, true);
        add.setGravity(Gravity.CENTER);
        add.setBackground(rounded(Color.rgb(38, 35, 60), ACCENT, 14));
        add.setPadding(dp(14), dp(14), dp(14), dp(14));
        add.setOnClickListener(v -> toggleBookmark());
        list.addView(add);

        List<Bookmark> bookmarks = loadBookmarks();
        if (bookmarks.isEmpty()) {
            TextView empty = label("Non hai ancora salvato preferiti.", 14, MUTED, false);
            empty.setGravity(Gravity.CENTER);
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, dp(100));
            list.addView(empty, params);
        }
        for (Bookmark bookmark : bookmarks) {
            LinearLayout row = new LinearLayout(this);
            row.setGravity(Gravity.CENTER_VERTICAL);
            row.setPadding(dp(14), dp(12), dp(8), dp(12));
            row.setBackground(rounded(CARD, BORDER, 14));
            LinearLayout.LayoutParams rowParams = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            rowParams.topMargin = dp(10);
            TextView name = label(bookmark.title + "\n" + domain(bookmark.url), 13, TEXT, true);
            row.addView(name, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
            row.addView(textButton("×", "Rimuovi preferito", () -> {
                removeBookmark(bookmark.url);
                render();
            }), new LinearLayout.LayoutParams(dp(40), dp(40)));
            row.setOnClickListener(v -> navigate(bookmark.url));
            list.addView(row, rowParams);
        }
        scroll.addView(list);
        content.addView(scroll, matchParent());
    }

    private void renderSettings() {
        ScrollView scroll = new ScrollView(this);
        LinearLayout page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(dp(18), dp(12), dp(18), dp(28));

        page.addView(sectionTitle("Ricerca"));
        TextView description = label("Scegli il motore usato quando cerchi dalla barra.", 13, MUTED, false);
        LinearLayout.LayoutParams descriptionParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        descriptionParams.topMargin = dp(6);
        page.addView(description, descriptionParams);

        String selected = preferences.getString("searchEngine", "duckduckgo");
        for (int i = 0; i < ENGINE_IDS.length; i++) {
            final String id = ENGINE_IDS[i];
            LinearLayout row = new LinearLayout(this);
            row.setGravity(Gravity.CENTER_VERTICAL);
            row.setPadding(dp(12), dp(4), dp(12), dp(4));
            row.setBackground(rounded(CARD, BORDER, 12));
            LinearLayout.LayoutParams rowParams = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, dp(52));
            rowParams.topMargin = dp(8);
            RadioButton radio = new RadioButton(this);
            radio.setButtonTintList(android.content.res.ColorStateList.valueOf(ACCENT));
            radio.setChecked(id.equals(selected));
            TextView name = label(ENGINE_NAMES[i], 14, TEXT, true);
            row.addView(radio, new LinearLayout.LayoutParams(dp(40), dp(48)));
            row.addView(name, new LinearLayout.LayoutParams(0,
                    ViewGroup.LayoutParams.WRAP_CONTENT, 1));
            View.OnClickListener choose = v -> {
                preferences.edit().putString("searchEngine", id).apply();
                render();
            };
            radio.setOnClickListener(choose);
            row.setOnClickListener(choose);
            page.addView(row, rowParams);
        }

        page.addView(sectionTitle("Privacy"));
        TextView privacy = label("Cosmic Android usa il WebView sicuro fornito dal sistema. "
                + "L'accesso ai file locali e ai contenuti misti non sicuri è disabilitato.",
                13, MUTED, false);
        privacy.setPadding(dp(14), dp(14), dp(14), dp(14));
        privacy.setBackground(rounded(CARD, BORDER, 14));
        LinearLayout.LayoutParams privacyParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        privacyParams.topMargin = dp(10);
        page.addView(privacy, privacyParams);

        page.addView(sectionTitle("Dati di navigazione"));
        TextView clear = label("Cancella cache, cookie e dati dei siti", 14, TEXT, true);
        clear.setGravity(Gravity.CENTER);
        clear.setPadding(dp(12), dp(14), dp(12), dp(14));
        clear.setBackground(rounded(CARD, BORDER, 14));
        clear.setOnClickListener(v -> confirmClearData());
        LinearLayout.LayoutParams clearParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        clearParams.topMargin = dp(10);
        page.addView(clear, clearParams);

        TextView about = label("Cosmic per Android · anteprima 0.2.0", 12, MUTED, false);
        about.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams aboutParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        aboutParams.topMargin = dp(24);
        page.addView(about, aboutParams);
        scroll.addView(page);
        content.addView(scroll, matchParent());
    }

    private TextView sectionTitle(String value) {
        TextView title = label(value, 16, TEXT, true);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.topMargin = dp(22);
        title.setLayoutParams(params);
        return title;
    }

    private void confirmClearData() {
        new AlertDialog.Builder(this)
                .setTitle("Cancellare i dati?")
                .setMessage("Saranno rimossi cache, cookie e dati locali dei siti.")
                .setNegativeButton("Annulla", null)
                .setPositiveButton("Cancella", (dialog, which) -> {
                    for (BrowserTab tab : tabs) {
                        tab.webView.clearCache(true);
                        tab.webView.clearHistory();
                        tab.webView.clearFormData();
                    }
                    CookieManager.getInstance().removeAllCookies(null);
                    CookieManager.getInstance().flush();
                    WebStorage.getInstance().deleteAllData();
                    Toast.makeText(this, "Dati di navigazione cancellati", Toast.LENGTH_SHORT).show();
                })
                .show();
    }

    private void navigate(String input) {
        String value = input == null ? "" : input.trim();
        if (value.isEmpty()) {
            return;
        }
        hideKeyboard();
        String destination;
        Uri uri = Uri.parse(value);
        String scheme = uri.getScheme();
        if ("http".equalsIgnoreCase(scheme) || "https".equalsIgnoreCase(scheme)) {
            destination = value;
        } else if (scheme != null && value.contains("://")) {
            Toast.makeText(this, "Questo tipo di indirizzo non è supportato", Toast.LENGTH_SHORT).show();
            return;
        } else if (!value.contains(" ") && (value.contains(".") || value.startsWith("localhost"))) {
            destination = "https://" + value;
        } else {
            destination = searchUrl(value);
        }
        activeTab.url = destination;
        activeTab.title = domain(destination);
        screen = Screen.BROWSER;
        render();
        activeTab.webView.loadUrl(destination);
    }

    private String searchUrl(String query) {
        String id = preferences.getString("searchEngine", "duckduckgo");
        for (int i = 0; i < ENGINE_IDS.length; i++) {
            if (ENGINE_IDS[i].equals(id)) {
                return ENGINE_URLS[i].replace("%s", Uri.encode(query));
            }
        }
        return ENGINE_URLS[0].replace("%s", Uri.encode(query));
    }

    private void updateChrome() {
        if (screen != Screen.BROWSER || root == null) {
            return;
        }
        if (addressField != null && !addressField.hasFocus()) {
            addressField.setText(activeTab.url);
        }
    }

    private void goBack() {
        if (activeTab.webView != null && activeTab.webView.canGoBack()) {
            activeTab.webView.goBack();
        } else if (!activeTab.url.isEmpty()) {
            showHome();
        }
    }

    private void goForward() {
        if (activeTab.webView != null && activeTab.webView.canGoForward()) {
            activeTab.webView.goForward();
        }
    }

    private void showHome() {
        activeTab.url = "";
        activeTab.title = "Nuova scheda";
        activeTab.webView.stopLoading();
        activeTab.webView.clearHistory();
        activeTab.webView.loadUrl("about:blank");
        screen = Screen.BROWSER;
        render();
    }

    private void closeTab(BrowserTab tab) {
        int index = tabs.indexOf(tab);
        if (index < 0) {
            return;
        }
        destroyTab(tab);
        tabs.remove(tab);
        if (tabs.isEmpty()) {
            activeTab = new BrowserTab("");
            createWebView(activeTab);
            tabs.add(activeTab);
        } else if (activeTab == tab) {
            activeTab = tabs.get(Math.max(0, index - 1));
        }
        screen = Screen.TABS;
        render();
    }

    private void destroyTab(BrowserTab tab) {
        if (tab.webView == null) {
            return;
        }
        if (tab.webView.getParent() instanceof ViewGroup) {
            ((ViewGroup) tab.webView.getParent()).removeView(tab.webView);
        }
        tab.webView.stopLoading();
        tab.webView.destroy();
        tab.webView = null;
    }

    private void toggleBookmark() {
        if (activeTab.url.isEmpty()) {
            Toast.makeText(this, "Apri una pagina per salvarla", Toast.LENGTH_SHORT).show();
            return;
        }
        if (isBookmarked(activeTab.url)) {
            removeBookmark(activeTab.url);
            Toast.makeText(this, "Rimosso dai preferiti", Toast.LENGTH_SHORT).show();
        } else {
            List<Bookmark> bookmarks = loadBookmarks();
            bookmarks.add(new Bookmark(activeTab.title, activeTab.url));
            saveBookmarks(bookmarks);
            Toast.makeText(this, "Aggiunto ai preferiti", Toast.LENGTH_SHORT).show();
        }
        render();
    }

    private boolean isBookmarked(String url) {
        if (url == null || url.isEmpty()) {
            return false;
        }
        for (Bookmark bookmark : loadBookmarks()) {
            if (bookmark.url.equals(url)) {
                return true;
            }
        }
        return false;
    }

    private List<Bookmark> loadBookmarks() {
        List<Bookmark> result = new ArrayList<>();
        try {
            JSONArray items = new JSONArray(preferences.getString("bookmarks", "[]"));
            for (int i = 0; i < items.length(); i++) {
                JSONObject item = items.getJSONObject(i);
                result.add(new Bookmark(item.optString("title", "Sito salvato"),
                        item.optString("url", "")));
            }
        } catch (JSONException ignored) {
            preferences.edit().remove("bookmarks").apply();
        }
        return result;
    }

    private void saveBookmarks(List<Bookmark> bookmarks) {
        JSONArray items = new JSONArray();
        for (Bookmark bookmark : bookmarks) {
            JSONObject item = new JSONObject();
            try {
                item.put("title", bookmark.title);
                item.put("url", bookmark.url);
                items.put(item);
            } catch (JSONException ignored) {
                // Fixed keys and strings cannot fail to serialize.
            }
        }
        preferences.edit().putString("bookmarks", items.toString()).apply();
    }

    private void removeBookmark(String url) {
        List<Bookmark> bookmarks = loadBookmarks();
        bookmarks.removeIf(bookmark -> bookmark.url.equals(url));
        saveBookmarks(bookmarks);
    }

    private void showMenu() {
        String[] items = {"Nuova scheda", "Preferiti", "Impostazioni", "Ricarica pagina"};
        new AlertDialog.Builder(this)
                .setTitle("Cosmic")
                .setItems(items, (dialog, which) -> {
                    switch (which) {
                        case 0:
                            createTab("");
                            break;
                        case 1:
                            screen = Screen.BOOKMARKS;
                            render();
                            break;
                        case 2:
                            screen = Screen.SETTINGS;
                            render();
                            break;
                        case 3:
                            if (!activeTab.url.isEmpty()) {
                                activeTab.webView.reload();
                            }
                            break;
                    }
                })
                .show();
    }

    private void hideKeyboard() {
        View focus = getCurrentFocus();
        if (focus != null) {
            ((InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE))
                    .hideSoftInputFromWindow(focus.getWindowToken(), 0);
            focus.clearFocus();
        }
    }

    private String domain(String url) {
        Uri uri = Uri.parse(url);
        String host = uri.getHost();
        return host == null || host.isEmpty() ? url : host.toLowerCase(Locale.ROOT);
    }

    private TextView textButton(String text, String description, Runnable action) {
        TextView button = label(text, text.length() > 2 ? 12 : 21, TEXT, false);
        button.setGravity(Gravity.CENTER);
        button.setContentDescription(description);
        button.setBackground(rounded(Color.TRANSPARENT, Color.TRANSPARENT, 14));
        button.setOnClickListener(v -> action.run());
        return button;
    }

    private TextView label(String text, int size, int color, boolean bold) {
        TextView view = new TextView(this);
        view.setText(text);
        view.setTextSize(size);
        view.setTextColor(color);
        if (bold) {
            view.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        }
        view.setGravity(Gravity.CENTER_VERTICAL);
        return view;
    }

    private GradientDrawable rounded(int color, int stroke, int radiusDp) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(dp(radiusDp));
        if (stroke != Color.TRANSPARENT) {
            drawable.setStroke(dp(1), stroke);
        }
        return drawable;
    }

    private LinearLayout.LayoutParams matchParent() {
        return new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT);
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    @Override
    public void onBackPressed() {
        if (screen != Screen.BROWSER) {
            screen = Screen.BROWSER;
            render();
            return;
        }
        if (activeTab.webView != null && activeTab.webView.canGoBack()) {
            activeTab.webView.goBack();
            return;
        }
        if (!activeTab.url.isEmpty()) {
            showHome();
            return;
        }
        super.onBackPressed();
    }

    @Override
    protected void onPause() {
        for (BrowserTab tab : tabs) {
            if (tab.webView != null) {
                tab.webView.onPause();
            }
        }
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        for (BrowserTab tab : tabs) {
            if (tab.webView != null && tab == activeTab) {
                tab.webView.onResume();
            }
        }
    }

    @Override
    protected void onStop() {
        JSONArray urls = new JSONArray();
        for (BrowserTab tab : tabs) {
            urls.put(tab.url);
        }
        preferences.edit()
                .putString("tabs", urls.toString())
                .putInt("activeTab", Math.max(0, tabs.indexOf(activeTab)))
                .apply();
        super.onStop();
    }

    @Override
    protected void onDestroy() {
        for (BrowserTab tab : new ArrayList<>(tabs)) {
            destroyTab(tab);
        }
        super.onDestroy();
    }

    private static final class BrowserTab {
        private String url;
        private String title = "Nuova scheda";
        private WebView webView;

        private BrowserTab(String url) {
            this.url = url;
        }
    }

    private static final class Bookmark {
        private final String title;
        private final String url;

        private Bookmark(String title, String url) {
            this.title = title == null || title.trim().isEmpty() ? url : title;
            this.url = url;
        }
    }
}

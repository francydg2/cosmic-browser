const repository = "francydg2/cosmic-browser";
const releasePage = `https://github.com/${repository}/releases/latest`;

async function loadLatestRelease() {
  const versionLabel = document.querySelector("#latest-version");
  const dateLabel = document.querySelector("#release-date");
  const note = document.querySelector("#download-note");

  try {
    const response = await fetch(`https://api.github.com/repos/${repository}/releases/latest`, {
      headers: { Accept: "application/vnd.github+json" }
    });
    if (!response.ok) throw new Error(`GitHub returned ${response.status}`);

    const release = await response.json();
    versionLabel.textContent = release.tag_name || "latest";
    if (release.published_at) {
      const date = new Date(release.published_at);
      dateLabel.textContent = ` · ${new Intl.DateTimeFormat("it-IT", { dateStyle: "medium" }).format(date)}`;
    }

    for (const card of document.querySelectorAll(".download-card[data-suffix]")) {
      const suffix = card.dataset.suffix;
      const asset = release.assets?.find((item) => item.name.endsWith(suffix));
      const link = card.querySelector("[data-download]");
      if (asset && asset.browser_download_url.startsWith(`https://github.com/${repository}/releases/download/`)) {
        link.href = asset.browser_download_url;
        link.setAttribute("download", "");
      } else {
        link.href = release.html_url || releasePage;
        note.textContent = "Il pacchetto non è disponibile per ogni sistema in questa release. Consulta le note su GitHub.";
      }
    }
  } catch {
    versionLabel.textContent = "vedi Releases";
    note.textContent = "Non è stato possibile aggiornare i link ora. Apri GitHub Releases per scaricare l'ultima versione.";
    for (const link of document.querySelectorAll("[data-download]")) link.href = releasePage;
  }
}

loadLatestRelease();

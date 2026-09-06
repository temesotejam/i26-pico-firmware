(async () => {
  try {
    const response = await fetch("firmware/manifest.json", { cache: "no-store" });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const data = await response.json();

    document.querySelector("#built-at").textContent = data.builtAt ?? "—";
    document.querySelector("#uf2-size").textContent = Number.isFinite(data.size)
      ? `${data.size.toLocaleString()} bytes`
      : "—";
    document.querySelector("#commit").textContent = data.sourceCommit
      ? data.sourceCommit.slice(0, 12)
      : "—";
  } catch (error) {
    document.querySelector("#built-at").textContent = "情報を取得できませんでした";
    console.error(error);
  }
})();

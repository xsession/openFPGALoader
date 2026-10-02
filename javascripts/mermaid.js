/* Render Mermaid fences emitted by pymdownx.superfences. */
document.addEventListener("DOMContentLoaded", () => {
  const blocks = document.querySelectorAll("pre.mermaid, code.language-mermaid");

  blocks.forEach((code) => {
    const pre = code.tagName === "PRE" ? code : code.closest("pre");
    if (!pre) return;

    const diagram = document.createElement("div");
    diagram.className = "mermaid";
    diagram.textContent = pre.textContent;
    pre.replaceWith(diagram);
  });

  if (window.mermaid) {
    window.mermaid.initialize({
      startOnLoad: false,
      securityLevel: "strict",
      theme: "neutral",
    });
    window.mermaid.run();
  }
});

document.addEventListener("DOMContentLoaded", function () {
  const blocks = document.querySelectorAll("pre code.language-mermaid");
  if (!blocks.length || !window.mermaid) return;
  const nodes = [];
  blocks.forEach(function (code) {
    const diagram = document.createElement("div");
    diagram.className = "mermaid";
    diagram.textContent = code.textContent;
    code.parentElement.replaceWith(diagram);
    nodes.push(diagram);
  });
  mermaid.initialize({ startOnLoad: false, securityLevel: "strict" });
  mermaid.run({ nodes: nodes });
});

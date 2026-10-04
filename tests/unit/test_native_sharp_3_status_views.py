from __future__ import annotations

import sys
import unittest
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[2] / "tools"
sys.path.insert(0, str(TOOLS))
import generate_native_sharp_3_status_views as views  # noqa: E402
import native_sharp_3_catalog as catalog  # noqa: E402


class NativeSharp3StatusViewTests(unittest.TestCase):
    def test_partition_is_exhaustive_and_terminal_only(self) -> None:
        items = [
            catalog.CatalogItem("NSC", "NS3-A", "a", "close a", "TERMINADA", "commit"),
            catalog.CatalogItem("NSC", "NS3-B", "b", "close b", "CERTIFICADA", "gate"),
            catalog.CatalogItem("NSLink", "NS3-C", "c", "close c", "PENDIENTE", "roadmap"),
            catalog.CatalogItem(
                "NSDBG", "NS3-D", "d", "close d", "PRUEBAS DIRIGIDAS", "11/11 checks"
            ),
            catalog.CatalogItem("NSASM", "NS3-E", "e", "close e", "REFERENCIA", "ledger"),
        ]

        finished, pending = views.partition_items(items)

        self.assertEqual([item.item_id for item in finished], ["NS3-A", "NS3-B"])
        self.assertEqual([item.item_id for item in pending], ["NS3-C", "NS3-D", "NS3-E"])
        self.assertEqual(
            {item.item_id for item in finished + pending},
            {item.item_id for item in items},
        )

    def test_pending_view_explains_dependencies_and_missing_evidence(self) -> None:
        items = [
            catalog.CatalogItem(
                "Integración",
                "NS3-I02",
                "gate",
                "Dependencias: T01–T06; cierre: layouts y debugger",
                "PENDIENTE",
                "Documento maestro §9",
            ),
            catalog.CatalogItem(
                "NSDBG",
                "NS3-DBG-12",
                "DAP",
                "contrato DAP reproducible",
                "PRUEBAS DIRIGIDAS",
                "11/11 dirigidas; falta certificación acumulada y baseline",
            ),
        ]

        rendered = views.render_pending(items, 2)

        self.assertIn("T01–T06", rendered)
        self.assertIn("falta certificación acumulada y baseline", rendered)
        self.assertIn("Condición exacta de cierre", rendered)

    def test_pending_view_surfaces_cross_item_and_technical_blockers(self) -> None:
        cross_item = catalog.CatalogItem(
            "NSC",
            "NS3-T08-F03",
            "closures",
            "complete closures after NS3-T08-F02",
            "EN CURSO",
            "active",
        )
        technical = catalog.CatalogItem(
            "NSC",
            "NS3-T06-F02",
            "partial moves",
            "move fields",
            "EN CURSO",
            "move-only generic field extraction sigue sin soporte; cierre pendiente",
        )

        self.assertIn("NS3-T08-F02", views.explicit_dependency(cross_item))
        self.assertIn("Bloqueo técnico", views.explicit_dependency(technical))
        self.assertIn("sin soporte", views.closure_obstacle(technical))

    def test_reference_is_not_reported_as_finished(self) -> None:
        item = catalog.CatalogItem(
            "Runtime/STL", "NS3-R001", "inventario", "conservar", "REFERENCIA", "ledger"
        )

        finished, pending = views.partition_items([item])

        self.assertEqual(finished, [])
        self.assertEqual(pending, [item])
        self.assertIn("decidir", views.closure_obstacle(item))


if __name__ == "__main__":
    unittest.main()

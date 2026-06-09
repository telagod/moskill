import { createSSRApp } from "vue";
import App from "./App.vue";
import store from "./store";
import MkIcon from "./components/MkIcon.vue";

export function createApp() {
  const app = createSSRApp(App);
  app.use(store);
  app.component("mk-icon", MkIcon);
  return { app };
}

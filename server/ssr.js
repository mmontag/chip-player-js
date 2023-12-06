import { renderToString } from "react-dom/server";
import App from "../src/components/App";
import { StaticRouter } from "react-router-dom";
import fs from 'fs';
import { Elysia } from 'elysia'
import { staticPlugin } from '@elysiajs/static'

const html = fs.readFileSync('./index-ssr.html', 'utf8');

function Component(props) {
  return (
    <body>
    <h1>{props.message}</h1>
    </body>
  );
}

const app = new Elysia()
  .use(staticPlugin({
    assets: '../public',
    prefix: '',
  }))
  .get('*', context => {
    const req = context.request;
    const app = renderToString(
      <StaticRouter location={new URL(req.url).pathname} context={{}}>
        {req.url}
        <App/>
      </StaticRouter>
    );
    const htm = html.replace('<!-- App -->', app);
    return new Response(htm, {
      headers: { "Content-Type": "text/html" },
    });
  })
  .listen(3000);

// Bun.serve({
//   fetch(req) {
//     const s = renderToString(
//       <StaticRouter location={new URL(req.url).pathname} context={{}}>
//         {req.url}
//         <App/>
//       </StaticRouter>
//     );
//     return new Response(s, {
//       headers: { "Content-Type": "text/html" },
//     });
//   },
//   port: 3000,
// });

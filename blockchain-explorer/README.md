# Blockchain Explorer + Transaction Dashboard

Professional React + Vite dashboard for exploring blockchain state, creating transactions, mining blocks, and visualizing peer topology.

## Stack

- React + Vite
- Axios
- Tailwind CSS
- react-force-graph

## Features

- Blockchain overview cards (blocks, transactions, difficulty, peers)
- Reverse-chronological block explorer with expandable transaction details
- Transaction creation panel with validation and API feedback
- Mining panel with loading state
- Network visualization graph (current node + peers)
- Auto-refresh every 3 seconds
- Robust error/loading handling

## Installation

```bash
cd blockchain-explorer
npm install
```

## Run (Development)

```bash
npm run dev
```

Open: `http://localhost:5173`

## Production Build

```bash
npm run build
npm run preview
```

## Backend Connection

By default, frontend connects to:

- `http://localhost:5000`

Create a `.env` file in `blockchain-explorer/` to customize:

```env
VITE_API_BASE_URL=http://localhost:5000
VITE_ADD_TX_ENDPOINT=/addTransaction
VITE_MINE_ENDPOINT=/mine
```

### Connect to a different node port

Example for node HTTP API on port `8001`:

```env
VITE_API_BASE_URL=http://localhost:8001
VITE_ADD_TX_ENDPOINT=/addTransaction
VITE_MINE_ENDPOINT=/mine
```

If your backend uses `/transaction/send` instead of `/addTransaction`, set:

```env
VITE_ADD_TX_ENDPOINT=/transaction/send
```

## Backend Test cURL Examples

### Get chain

```bash
curl http://localhost:5000/chain
```

### Get peers

```bash
curl http://localhost:5000/peers
```

### Add transaction

```bash
curl -X POST http://localhost:5000/addTransaction \
  -H "Content-Type: application/json" \
  -d '{
    "senderPublicKey":"sender_key",
    "receiverPublicKey":"receiver_key",
    "payload":"Hackathon transaction",
    "amount":10
  }'
```

### Mine block

````bash
curl -X POST http://localhost:5000/mine \
  -H "Content-Type: application/json" \
  -d '{}'
```# React + Vite

This template provides a minimal setup to get React working in Vite with HMR and some ESLint rules.

Currently, two official plugins are available:

- [@vitejs/plugin-react](https://github.com/vitejs/vite-plugin-react/blob/main/packages/plugin-react) uses [Babel](https://babeljs.io/) (or [oxc](https://oxc.rs) when used in [rolldown-vite](https://vite.dev/guide/rolldown)) for Fast Refresh
- [@vitejs/plugin-react-swc](https://github.com/vitejs/vite-plugin-react/blob/main/packages/plugin-react-swc) uses [SWC](https://swc.rs/) for Fast Refresh

## React Compiler

The React Compiler is not enabled on this template because of its impact on dev & build performances. To add it, see [this documentation](https://react.dev/learn/react-compiler/installation).

## Expanding the ESLint configuration

If you are developing a production application, we recommend using TypeScript with type-aware lint rules enabled. Check out the [TS template](https://github.com/vitejs/vite/tree/main/packages/create-vite/template-react-ts) for information on how to integrate TypeScript and [`typescript-eslint`](https://typescript-eslint.io) in your project.
````

import { Routes,Route } from "react-router"
import RecipientDisplay from "./components/DisplayBantuan"
import Loket from "./components/Loket"


const App = () => {
  return (
    <Routes>
      <Route element={<Loket/>} path="/loket"></Route>
      <Route element={<RecipientDisplay/>} path="/antrian"></Route>
    </Routes>
  )
}

export default App